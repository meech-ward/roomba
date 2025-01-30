import machine
import time
import utime
from .OV2640_reg import (    
    OV2640_JPEG_INIT ,
    OV2640_YUV422,
    OV2640_JPEG,
    OV2640_160x120_JPEG ,
    OV2640_176x144_JPEG,
    OV2640_320x240_JPEG,
    OV2640_352x288_JPEG,
    OV2640_640x480_JPEG,
    OV2640_800x600_JPEG,
    OV2640_1024x768_JPEG,
    OV2640_1280x1024_JPEG,
    OV2640_1600x1200_JPEG
)


# Arducam internal registers for FIFO
REG_FIFO        = const(0x04)
FIFO_CLEAR_MASK = const(0x01)
FIFO_START_MASK = const(0x02)
REG_TRIG        = const(0x41)
CAP_DONE_MASK   = const(0x08)

# FIFO size registers
FIFO_SIZE1      = const(0x42)
FIFO_SIZE2      = const(0x43)
FIFO_SIZE3      = const(0x44)

class ArduCam:
    def __init__(
        self,
        spi_id=0,      # Using SPI0
        sck_pin=2,
        mosi_pin=3,
        miso_pin=4,
        cs_pin=5,
        i2c_id=0,      # Using I2C0
        scl_pin=9,
        sda_pin=8,
        sensor_addr=0x30,  # OV2640 default I2C address
        buffer_size=2048*4
    ):
        self._burst_buf = bytearray(buffer_size)

        # Setup CS (chip select)
        self.cs = machine.Pin(cs_pin, machine.Pin.OUT, value=1)

        # Setup SPI
        self.spi = machine.SPI(
            spi_id,
            baudrate=8_000_000,  
            # baudrate=4_000_000,   # You can try higher if stable
            polarity=0,
            phase=0,
            bits=8,
            firstbit=machine.SPI.MSB,
            sck=machine.Pin(sck_pin),
            mosi=machine.Pin(mosi_pin),
            miso=machine.Pin(miso_pin),
        )

        # Setup I2C
        self.i2c = machine.I2C(
            i2c_id,
            scl=machine.Pin(scl_pin),
            sda=machine.Pin(sda_pin),
            # freq=100_000
            freq=400_000  # Up from 100kHz
        )
        self.sensor_addr = sensor_addr
        utime.sleep_ms(100)

    # -----------
    # SPI helpers
    # -----------
    def _spi_write_reg(self, reg, val):
        wbuf = bytearray([reg | 0x80, val])
        self.cs.value(0)
        self.spi.write(wbuf)
        self.cs.value(1)

    def _spi_read_reg(self, reg):
        wbuf = bytearray([reg & 0x7F])
        rbuf = bytearray(1)
        self.cs.value(0)
        self.spi.write(wbuf)
        self.spi.readinto(rbuf, 0x00)
        self.cs.value(1)
        return rbuf[0]

    # ------------
    # I2C helpers
    # ------------
    def _i2c_write_reg8_8(self, reg_addr, val):
        buf = bytearray([reg_addr & 0xFF, val & 0xFF])
        self.i2c.writeto(self.sensor_addr, buf)
        utime.sleep_ms(2)

    def _i2c_read_reg8_8(self, reg_addr):
        self.i2c.writeto(self.sensor_addr, bytearray([reg_addr & 0xFF]), stop=False)
        rbuf = self.i2c.readfrom(self.sensor_addr, 1)
        return rbuf[0]

    # ----------------
    # FIFO Management
    # ----------------
    def flush_fifo(self):
        self._spi_write_reg(REG_FIFO, FIFO_CLEAR_MASK)

    def start_capture(self):
        self._spi_write_reg(REG_FIFO, FIFO_START_MASK)

    def capture_done(self):
        trig = self._spi_read_reg(REG_TRIG)
        return bool(trig & CAP_DONE_MASK)

    def fifo_length(self):
        l1 = self._spi_read_reg(FIFO_SIZE1)
        l2 = self._spi_read_reg(FIFO_SIZE2)
        l3 = self._spi_read_reg(FIFO_SIZE3) & 0x7F
        return (l3 << 16) | (l2 << 8) | l1

    # if memory allows. Fewer loop iterations means less overhead in Python.
    def burst_read_fifo(self):
    # def burst_read_fifo(self, chunk_size=512):
        length = self.fifo_length()
        if length == 0:
            return b''

        # Start burst read
        self.cs.value(0)
        self.spi.write(bytearray([0x3C]))  # Burst read command

        # We'll accumulate the entire JPEG in memory.
        # If you plan to stream it chunk by chunk, you can handle that differently.
        result = bytearray()
        bytes_left = length

        while bytes_left > 0:
            read_size = min(bytes_left, len(self._burst_buf))
            # use the pre-allocated buffer
            self.spi.readinto(self._burst_buf, 0xFF)
            # Append only the portion we actually read
            result.extend(self._burst_buf[:read_size])
            bytes_left -= read_size

        self.cs.value(1)
        return result


    # -------------
    # Helper to load registers
    # -------------
    def load_regs(self, reg_list):
        for (reg, val) in reg_list:
            if reg == 0xff and val == 0xff:
                break
            self._i2c_write_reg8_8(reg, val)

    # -------------------------
    # OV2640 configuration
    # -------------------------
    def init_ov2640_jpeg_320x240(self):
        """
        Initialize the OV2640 for 320x240 (QVGA) JPEG output.
        """
        # Soft reset
        self._i2c_write_reg8_8(0xff, 0x01)
        self._i2c_write_reg8_8(0x12, 0x80)
        self._i2c_write_reg8_8(0x4407, 0x04); # default quality

        utime.sleep_ms(100)

        # 1) Basic JPEG init
        self.load_regs(OV2640_JPEG_INIT)
        # 2) YUV422
        self.load_regs(OV2640_YUV422)
        # 3) JPEG mode
        self.load_regs(OV2640_JPEG)
        # 4) Then set QVGA resolution
        self.load_regs(OV2640_320x240_JPEG)

        print("OV2640: Initialized for 320x240 JPEG.")

    def init_ov2640_jpeg_640x480(self):
        """
        Initialize the OV2640 for 640x480 (VGA) JPEG output.
        """
        self._i2c_write_reg8_8(0xff, 0x01)
        self._i2c_write_reg8_8(0x12, 0x80)
        self._i2c_write_reg8_8(0x4407, 0x08); # low quality not working

        utime.sleep_ms(100)

        self.load_regs(OV2640_JPEG_INIT)
        self.load_regs(OV2640_YUV422)
        self.load_regs(OV2640_JPEG)
        self.load_regs(OV2640_640x480_JPEG)

        print("OV2640: Initialized for 640x480 JPEG.")

    def init_ov2640_jpeg_1600x1200(self):
        """
        Initialize the OV2640 for 1600x1200 (UXGA) JPEG output.
        """
        self._i2c_write_reg8_8(0xff, 0x01)
        self._i2c_write_reg8_8(0x12, 0x80)
        utime.sleep_ms(100)

        self.load_regs(OV2640_JPEG_INIT)
        self.load_regs(OV2640_YUV422)
        self.load_regs(OV2640_JPEG)
        self.load_regs(OV2640_1600x1200_JPEG)

        print("OV2640: Initialized for 1600x1200 JPEG.")

    def single_capture(self, timeout_ms=2000, flush=True):
        """
        Flush FIFO, start capture, wait for done, read JPEG data.
        Returns the raw JPEG bytes or None if timed out.
        """
        if flush:
            self.flush_fifo()
        self.start_capture()

        t0 = utime.ticks_ms()
        while not self.capture_done():
            if utime.ticks_diff(utime.ticks_ms(), t0) > timeout_ms:
                print("Capture timeout!")
                return None
            utime.sleep_ms(1)

        return self.burst_read_fifo()
