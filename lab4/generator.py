import sys
import random
import serial
import time

def main():
    if len(sys.argv) != 2:
        print("Please enter a serial port as 1st param")
        sys.exit(1)

    COM_PORT = sys.argv[1]

    try:
        if not serial.serial_for_url(COM_PORT).is_open:
            print("Serial port is not available or does not exist.")
            sys.exit(1)

        
        with serial.Serial(COM_PORT) as ser:
            while True:
                temperature = random.uniform(-30, 35)
                data = "{:.2f}\n".format(temperature).encode('utf-8')
                ser.write(data)
                print("Temperature sent:", data.decode('utf-8').strip())
                time.sleep(5)

    except serial.SerialException as e:
        print("Serial Exception:", e)
        sys.exit(1)
    except Exception as e:
        print("Exception:", e)
        sys.exit(1)

if __name__ == "__main__":
    main()
