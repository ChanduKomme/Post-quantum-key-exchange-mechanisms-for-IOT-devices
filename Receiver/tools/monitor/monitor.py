#!/bin/env python3

import argparse
import errno
import os
import serial
import stat
import struct
import time
import threading
import tkinter as tk
from tkinter import messagebox
from tkinter.filedialog import asksaveasfile
from tkinter.scrolledtext import ScrolledText
from tkinter import ttk

class Application(tk.Tk):
    def __init__(self, device, baudrate, fifoPath):
        # Init window
        super().__init__()

        # Create configuration
        self.conf = {
            "device": device,
            "baudrate": baudrate,
            "fifoPath": fifoPath
        }

        # Apply new title
        self.title(f"BL602 Monitor - {device}")

        # Set height, width and position
        screenWidth = self.winfo_screenwidth()
        screenHeight = self.winfo_screenheight()
        rootWidth = int(screenHeight * 16 / 9 / 2)
        rootHeight = int(screenHeight / 2)
        centerX = int(screenWidth/2 - rootWidth/2)
        centerY = int(screenHeight/2 - rootHeight/2)
        self.geometry(f"{rootWidth}x{rootHeight}+{centerX}+{centerY}")

        # Create menu bar
        menubar = tk.Menu(self)
        self.config(menu=menubar)
        fileMenu = tk.Menu(menubar, tearoff=False)
        fileMenu.add_command(label="Save serial log", command=self.saveToFile)
        fileMenu.add_command(label="Exit", command=self.exit)
        menubar.add_cascade(label="File", menu=fileMenu, underline=0)

        # Override x button
        self.protocol("WM_DELETE_WINDOW", self.exit)

        # Create serial text output view
        self.text = ScrolledText(self, width=80, height=15)
        self.text.configure(state="disabled")
        self.text.pack(padx = 10, pady = (10, 5), fill=tk.BOTH, expand=True, anchor=tk.CENTER)

        # Create serial text input
        self.serialInputEntry = ttk.Entry(self)
        self.serialInputEntry.pack(padx = (10, 5), pady=(5, 10), side=tk.LEFT, fill=tk.BOTH, expand=True, anchor=tk.S)
        self.serialInputEntry.bind("<Return>", self.sendSerialBind) # Send text when pressing return

        # Create send serial text button
        self.sendSerialInput = ttk.Button(self, text="Send", command=self.sendSerial)
        self.sendSerialInput.pack(padx = (5, 10), pady = (5, 10), fill=tk.X, anchor=tk.SE)

        # Create PCAP output object
        self.pcapOutput = BinaryToPCAP(self)

        # Start reading serial console
        self.serialConsoleReading = 1
        thread = threading.Thread(target=self.readSerial, name="Serial monitor thread")
        thread.start()

        # Buffer to write data
        self.serialOutputBuffer = ""

    def saveToFile(self):
        # Open save file dialog
        fileName = asksaveasfile(mode="w", defaultextension=".txt", filetypes=[("Text documents", "*.txt")])

        # Dialog dismissed -> return
        if fileName is None:
            return
        
        # Store data
        text = str(self.text.get(1.0, tk.END))
        fileName.write(text)
        fileName.close()

    def readSerial(self):
        try:
            with serial.Serial(self.conf["device"], self.conf["baudrate"],
                bytesize=serial.EIGHTBITS, parity = serial.PARITY_NONE,
                stopbits = serial.STOPBITS_ONE, xonxoff = False, rtscts= False, timeout=1) as ser:

                ser.reset_input_buffer()
                ser.reset_output_buffer()

                while self.serialConsoleReading:
                    # Write output data from self.serialOutputBuffer if present
                    if len(self.serialOutputBuffer) > 0:
                        # Encode into ascii
                        serialString = self.serialOutputBuffer.encode('utf-8')

                        # Clear buffer
                        self.serialOutputBuffer = ""

                        # Send text
                        test = ser.write(serialString)
                    
                    # Read line
                    line = ser.readline()
                    line = line.strip()
                    lineLen = len(line)

                    # Write line to output if text is present
                    if lineLen > 1:
                        try:
                            line = line.decode("utf-8")

                            # Check if line is packet
                            if line.startswith("#pkt#"):
                                self.pcapOutput.handleReceivedFrame(line[5:])
                            else:
                                # Output line
                                line = f"{line}\n"
                                self.text.configure(state="normal")
                                self.text.insert("end", line)
                                self.text.configure(state="disabled")
                                self.text.see("end")
                        except UnicodeDecodeError as e:
                            print(f"Could not decode line: {line}")
                            print(f"Exception: {e}")
        except KeyboardInterrupt as e:
            self.exit()
        except RuntimeError as e:
            self.exit()
        except serial.SerialException as e:
            confirmation = messagebox.showerror("Serial device not found", f"The specified serial device ({self.conf["device"]}) does not exist")
            os._exit(1) # here we terminate everything directly
        except Exception as e:
            print(f"Could not open port: {e}")

    # Binding to send text if enter is pressed
    def sendSerialBind(self, textEntry):
        self.sendSerial()

    # Binding to send text if button is pressed
    def sendSerial(self):
        # Get current content and sent store in buffer which is evaluated in read function
        self.serialOutputBuffer = self.serialInputEntry.get() + "\r\n"

        # Clear input entry
        self.serialInputEntry.delete(0, tk.END)

    # Exit function
    def exit(self):
        self.serialConsoleReading = 0 # Stop reading serial console
        try:
            self.destroy() # Destroy GUI
        except Exception:
            pass
    
    # Get configuration
    def getConfiguration(self):
        return self.conf

# motivated from https://github.com/g-oikonomou/sensniff/blob/master/sensniff.py
class BinaryToPCAP():
    def __init__(self, application):
        # Generate header https://wiki.wireshark.org/Development/LibpcapFileFormat

        # Constants
        self.DLT_TYPE = 101 # Link layer packet type (here we set it to IP wihtout link-layer header as we can not capture this one with lwIP)
        # See https://www.tcpdump.org/linktypes.html for all types
        self.PCAP_GLOBAL_HEADER_FORMAT = "<LHHlLLL"
        self.PCAP_FRAME_HEADER_FORMAT = "<LLLL"

        PCAP_MAGIC_NUMBER = 0xA1B2C3D4
        PCAP_VERSION_MAJOR = 2
        PCAP_VERSION_MINOR = 4
        PCAP_TIMEZONE = 0
        PCAP_SIGFIGS = 0
        PCAP_SNAPLEN = 65535
        PCAP_NETWORK = self.DLT_TYPE

        self.app = application
        self.outputFifoPath = self.app.getConfiguration()["fifoPath"]

        # Create global header
        self.PCAP_GLOBAL_HEADER = bytearray(struct.pack(self.PCAP_GLOBAL_HEADER_FORMAT,
            PCAP_MAGIC_NUMBER,
            PCAP_VERSION_MAJOR,
            PCAP_VERSION_MINOR,
            PCAP_TIMEZONE,
            PCAP_SIGFIGS,
            PCAP_SNAPLEN,
            PCAP_NETWORK))

        self.createdFifo = False
        self.outputFifo = None
        self.requiresPCAPHeader = True
        self.createFifo()

    def createFifo(self):
        # Create fifo queue
        try:
            if not self.createdFifo:
                self.createdFifo = True
                os.mkfifo(self.outputFifoPath)
        except OSError as e:
            if e.errno == errno.EEXIST:
                if stat.S_ISFIFO(os.stat(self.outputFifoPath).st_mode) is False:
                    print(f"File {self.outputFifoPath} exists and is not a FIFO queue")
                else:
                    print(f"Queue {self.outputFifoPath} already exists - reusing it")
            else:
                print(f"Can not create FIFO queue at {self.outputFifoPath}: {e}")
                self.app.exit()

    def openFifoQueue(self):
        # Open fifo queue
        if self.outputFifo is None:
            try:
                fileDescriptor = os.open(self.outputFifoPath, os.O_NONBLOCK | os.O_WRONLY)
                self.outputFifo = os.fdopen(fileDescriptor, "wb")
            except OSError as e:
                print("Could not open FIFO queue, is Wireshark reading?")
                self.outputFifo = None
                self.requiresPCAPHeader = True

    def handleReceivedFrame(self, raw):
        # Create timestamp
        timestamp = time.time()
        seconds = int(timestamp)
        microseconds = int((timestamp - seconds) * 1000000)

        # Generate PCAP        
        try:
            # Filter out packets with invalid header length (< 20), should be always 20 with lwIP
            try:
                if int(raw[1]) < 5: # 5 equals 20 in the representation the string comes in
                    print("Dropping packet with invalid header")
                    return
            except ValueError as e:
                print("Dropping packet with invalid header")
            
            # Sometimes it occurs a packet has an odd number of characters.
            # This should not happen, because this means we received only half a byte (four bits) somewhere when converting the string to a byte
            # As workaround, we append a zero to ensure the length matches with the expected one.
            # The assumption here is that the last byte contains no meaningful information.
            # In some cases it might be better to drop the packet instead (revisit this in the future after some testing).
            # Apparently, this affects especially network management protocols (BOOTP, IGMP, MDNS)
            if (len(raw) % 2 != 0):
                print("Suspicious body len, appending a zero")
                raw = raw + "0"

            body = bytearray.fromhex(raw)

            # Measure length of body
            length = len(body)

            # Generate PCAP header
            header = bytearray(struct.pack(self.PCAP_FRAME_HEADER_FORMAT, seconds, microseconds, length, length))

            pcap = header + body
            
            # Pipe PCAP
            if self.createdFifo is False:
                self.createFifo()

            if self.outputFifo is None:
                self.openFifoQueue()
            
            if self.outputFifo is not None: 
                try:
                    if self.requiresPCAPHeader is True:
                        self.requiresPCAPHeader = False
                        self.outputFifo.write(self.PCAP_GLOBAL_HEADER)

                    self.outputFifo.write(pcap)
                    self.outputFifo.flush()
                except IOError as e:
                    if e.errno == errno.EPIPE:
                        print("Wireshark stopped reading")
                        self.outputFifo = None
                        self.requiresPCAPHeader = True
        except Exception as e:
            print(f"Dropped a packet that could not be converted to pcap: {e}")

if __name__ == "__main__":
    # Read command line parameters
    parser = argparse.ArgumentParser(description="BL602 serial monitor")
    parser.add_argument("-d", "--device", default="/dev/ttyUSB0", help="Serial device to connect to (default: /dev/ttyUSB0).")
    parser.add_argument("-b", "--baudrate", default=2000000, help="Baudrate used for communication (default: 2000000).")
    parser.add_argument("-f", "--fifoPath", default="/tmp/sniff", help="Path to the FIFO queue used to pipe captured network traffic (default: /tmp/sniff).")
    args = parser.parse_args()

    # Start application
    app = Application(args.device, args.baudrate, args.fifoPath)
    try:
        app.mainloop()
    except KeyboardInterrupt as e:
        app.exit()
    