"""Tests of production ND7 NDSIF framing, receiver and hardware routing."""
import binascii
import ctypes
from pathlib import Path
import struct
import sys
import unittest

D = ctypes.CDLL(str(Path(sys.argv.pop(1)).resolve()))
D.feed.argtypes = [ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint]
D.write_at.restype = ctypes.c_uint


def encode(raw):
    result = bytearray([0]); pos = 0; code = 1
    for byte in raw:
        if not byte:
            result[pos] = code; pos = len(result); result.append(0); code = 1
        else:
            result.append(byte); code += 1
            if code == 255:
                result[pos] = code; pos = len(result); result.append(0); code = 1
    result[pos] = code
    return b'\0' + result + b'\0'


def frame(op, payload=b'', kind=0, request=0x1234):
    raw = b'ND' + bytes([1, kind, op]) + struct.pack('<HH', request, len(payload)) + payload
    return encode(raw + struct.pack('<H', binascii.crc_hqx(raw, 0xffff)))


def receive():
    buffer = ctypes.create_string_buffer(32768)
    count = D.output(buffer)
    replies = []
    for encoded in filter(None, buffer.raw[:count].split(b'\0')):
        raw = bytearray(); offset = 0
        while offset < len(encoded):
            code = encoded[offset]; offset += 1
            raw.extend(encoded[offset:offset+code-1]); offset += code-1
            if code != 255 and offset < len(encoded): raw.append(0)
        assert raw[:4] == b'ND\1\2'
        assert binascii.crc_hqx(raw[:-2], 0xffff) == struct.unpack('<H', raw[-2:])[0]
        assert len(raw) == 11 + struct.unpack('<H', raw[7:9])[0]
        replies.append((raw[4], struct.unpack('<H', raw[5:7])[0], bytes(raw[9:-2])))
    return replies


class ProtocolTests(unittest.TestCase):
    def setUp(self):
        D.init()
        self.assertEqual(D.metric(9), 0, 'Firmware configASSERT failed')

    def feed(self, data, now=1, rounds=100):
        D.feed(data, len(data), now); D.pump(rounds)

    def send(self, op, payload=b'', kind=0):
        self.feed(frame(op, payload, kind)); return receive()

    def write(self, op, instance=0, page=0, pairs=b'\x20\xc7'):
        self.assertEqual(self.send(op, bytes([instance, page])+pairs), [])

    def writes(self):
        return [tuple(struct.pack('<I', D.write_at(i))) for i in range(D.metric(1))]

    def clock(self, op, hz, instance=0):
        return self.send(4, bytes([op, instance])+struct.pack('<I', hz), 1)[0][2]

    def test_identity(self):
        data = self.send(2, kind=1)[0][2]
        n = data[1]
        self.assertEqual(data[:2+n], b'\0\x0bNanoDrive 7')
        self.assertEqual(data[3+n:], b'4.0b')

    def test_capabilities_and_pagination(self):
        data = self.send(5, b'\0\0', 1)[0][2]
        self.assertEqual(struct.unpack('<BHIHHHB', data[:14]), (0,256,0,0,1,65535,10))
        entries = [struct.unpack('<BBBBBI', data[i:i+9]) for i in range(14, len(data), 9)]
        self.assertEqual([(e[0],e[1],e[2]) for e in entries],
                         [(0x55,0,0),(0x55,1,0),(0xa5,1,0),(0x51,0,0),
                          (0x5e,0,0),(0x5f,0,1),(0xa0,0,0),(0xa0,1,0),(0x5a,0,0),(0x5b,0,0)])
        self.assertEqual(entries[3][3:], (1,1,3579540))  # BURST, CLOCK_FIXED, logical Hz
        self.assertTrue(all(e[3] == 5 and e[4] == 2 for e in entries if e[0] != 0x51))
        self.assertEqual(self.send(5, b'\x0a\0', 1)[0][2][-1], 0)
        self.assertEqual(self.send(5, b'\x0b\0', 1)[0][2], b'\1')

    def test_all_native_physical_routes(self):
        for op, inst, page in [(0x55,0,0),(0x55,1,0),(0xa5,1,0),(0x51,0,0),(0x5e,0,0),(0x5f,0,1)]:
            self.write(op, inst, page)
        self.assertEqual(self.writes(), [(0,0,0x20,0xc7),(1,0,0x20,0xc7),(1,0,0x20,0xc7),
                                        (3,0,0x20,0xc7),(2,0,0x20,0xc7),(2,1,0x20,0xc7)])

    def test_burst_maximum_and_order(self):
        pairs = b''.join(bytes([i, 255-i]) for i in range(127))
        self.write(0x55, 1, pairs=pairs)
        self.assertEqual(self.writes(), [(1,0,i,255-i) for i in range(127)])
        self.assertEqual(D.metric(11), 1, 'Refresh keyboard once per burst')

    def test_ay_alias_and_frame_atomic_validation(self):
        self.write(0xa0, 0, pairs=b'\0\x42')
        self.write(0xa0, 1, pairs=b'\x0f\x13')
        self.assertEqual(self.writes(), [(0,0,0,0x42),(1,0,15,0x13)])
        for bad in [b'\0\x99\x10\x44', b'\x80\x11']:
            self.write(0xa0, pairs=bad)
        self.assertEqual(D.metric(1), 2, 'No partial writes before invalid last pair')

    def test_opll_whole_burst_address_validation(self):
        self.write(0x51, pairs=b'\x38\x10')
        self.write(0x51, pairs=b'\x20\x01\x40\x02')
        self.assertEqual(self.writes(), [(3,0,0x38,0x10)])

    def test_invalid_targets_and_types_never_reply(self):
        for op, inst, page in [(0x55,2,0),(0x55,0,1),(0xa5,0,0),(0x51,1,0),
                               (0x5e,0,1),(0x5f,0,0),(0x5f,1,1),(0xa0,2,0)]:
            self.write(op,inst,page)
        for payload in [b'',b'\0\0',b'\0\0\x20']:
            self.assertEqual(self.send(0x55,payload),[])
        self.assertEqual(self.send(0x55,b'\0\0\x20\x12',1),[])
        self.assertEqual(D.metric(1),0)

    def test_unsupported_and_pcm_requests(self):
        for op in [0x30,0x50,0x52,0x53,0x54,0x56,0x57,0x58,0x59,0xb7,0xd0]:
            self.write(op)
            self.assertEqual(self.send(op,b'\0\0\x20\x12',1),[])
        for op in [0x10,0x11,0x12]: self.assertEqual(self.send(op,b'\xb7\0'),[])
        self.assertEqual(self.send(0x13,b'\xb7\0',1)[0][2],b'\1')
        self.assertEqual(D.metric(1),0)

    def test_clock_mapping_and_shared_opn(self):
        self.assertEqual(self.clock(0x55,3000000,1),b'\0')
        self.assertEqual(D.metric(3),3000000)
        self.assertEqual(self.clock(0xa5,4000000,1),b'\0')
        self.assertEqual(self.clock(0xa0,1250000,1),b'\0')
        self.assertEqual(D.metric(3),2500000)
        self.assertEqual(self.clock(0x5a,3579545),b'\0')
        self.assertEqual(D.metric(4),14318180)
        self.assertEqual(self.clock(0x5b,4000000),b'\0')
        self.assertEqual(D.metric(4),16000000)
        self.assertEqual(self.clock(0x5e,14318180),b'\0')
        self.assertEqual(D.metric(4),14318180, 'Native OPL3 must not be multiplied again')
        self.assertEqual(self.clock(0x5f,12000000),b'\0')
        self.assertEqual(D.metric(4),12000000)

    def test_clock_rejections_leave_hardware_unchanged(self):
        for op,hz,inst in [(0x51,3579540,0),(0x55,0,0),(0x55,1234567,0),(0x55,0x40000000+4000000,0),
                            (0xa0,0xffffffff,0),(0x5b,0xffffffff,0),(0xa5,4000000,0),
                            (0x5e,14318180,1),(0x54,4000000,0)]:
            self.assertEqual(self.clock(op,hz,inst),b'\1')
        self.assertEqual(D.metric(2),0)

    def test_opl_compatibility_mode_transition_order(self):
        self.write(0x5f, page=1, pairs=b'\5\1')
        self.write(0x5a, pairs=b'\x20\x41')
        self.assertEqual(self.writes(), [(2,1,5,1),(2,1,5,0),(2,0,0x20,0x41)])

    def test_reset_and_volume(self):
        self.assertEqual(self.send(3,b'\x18',1)[0][2], b'\0')
        self.assertEqual(self.send(0,kind=1)[0][2], b'\0')
        self.assertEqual(D.metric(0),1)
        self.assertEqual(D.metric(5),24)
        self.assertEqual(self.send(3,b'\x61',1)[0][2], b'\1')
        self.assertEqual(D.metric(5),24)

    def test_echo_multiple_replies_and_partial_tx(self):
        D.tx_space(1)
        data = frame(1,bytes(range(32)),1,7)+frame(1,b'next',1,8)
        self.feed(data, rounds=180)
        self.assertEqual(receive(), [(1,7,b'\0'+bytes(range(32))),(1,8,b'\0next')])

    def test_fragmented_and_coalesced_bursts(self):
        data=frame(0x55,b'\0\0\x20\x34')
        for byte in data: self.feed(bytes([byte]),now=10)
        self.feed(data*10)
        self.assertEqual(D.metric(1),11)
        self.assertEqual(receive(),[])

    def test_corrupt_frame_and_timeout_recovery(self):
        data=frame(0x55,b'\0\0\x20\x34')
        bad=bytearray(data);bad[-2]^=1
        self.feed(bytes(bad)); self.feed(b'\0\xff\x01\0')
        self.feed(data[:5],now=10);self.feed(data[5:],now=600)
        self.assertEqual(D.metric(1),0)
        self.feed(data,now=601)
        self.assertEqual(D.metric(1),1)

    def test_transient_sof_loss_keeps_registers_and_buffer(self):
        D.connection(0,0)
        self.write(0x55)
        self.assertEqual(D.metric(1),1)
        self.assertEqual(D.metric(0),0)
        self.feed(frame(1,b'pending',1))
        self.assertEqual(receive(),[])
        D.connection(1,1);D.pump(100)
        self.assertEqual(receive()[0][2],b'\0pending')

    def test_usb_bus_reset_discards_old_bytes_and_recovers(self):
        data=frame(0x55,b'\0\0\x20\x34')
        D.feed(data,len(data),10);D.bus_reset();D.pump(20)
        self.assertEqual(D.metric(1),1)  # only explicit OPL3 NEW=0 during reset
        self.assertEqual(D.metric(0),1)
        self.feed(data)
        self.assertEqual(self.writes()[-1],(0,0,0x20,0x34))

    def test_receive_overflow_stops_and_recovers(self):
        D.tx_space(0)
        self.feed(frame(1,b'blocked',1))
        self.feed(b'\x11'*12000, rounds=40)
        self.assertGreater(D.metric(6),0)
        self.assertGreater(D.metric(8),0)
        self.assertGreater(D.metric(0),0)
        D.tx_space(512)
        self.write(0x55,pairs=b'\x20\x67')
        self.assertEqual(self.writes()[-1],(0,0,0x20,0x67))

    def test_explicit_discard_resynchronizes(self):
        data=frame(0x55,b'\0\0\x20\x34')
        self.feed(data[:5]);D.discard();D.pump(1);self.feed(data[5:])
        self.assertEqual(D.metric(0),1)
        self.feed(data)
        self.assertEqual(self.writes()[-1],(0,0,0x20,0x34))

    def test_original_nd8_callback_api_remains_usable(self):
        D.legacy()
        self.write(0x54)
        self.assertEqual(self.writes(),[(0,0,0x20,0xc7)])
        data=self.send(2,kind=1)[0][2]
        self.assertIn(b'NanoDrive 8',data)


if __name__ == '__main__': unittest.main()
