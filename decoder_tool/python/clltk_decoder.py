#!/usr/bin/env python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# %%
import re
import json
import sys
import logging
import struct  # to get float and double from binary
from datetime import datetime
import pytz

import warnings
warnings.filterwarnings("ignore", message="The default behavior of tarfile extraction has been changed", module="tarfile")
import tarfile

from enum import Enum
from hashlib import md5
from sys import exit
import unicodedata
from decimal import *
import concurrent.futures
import traceback
import os
getcontext().prec = 28 # set decimal precision

class MetaEntryType(Enum):
    Printf = 1
    Dump = 2
    
class Endian(Enum):
    Big = "big"
    Little = "little"

current_Endian : Endian = None

current_version = (0,0) # major, minor

def get_const(name:str):
    global current_version
    cv = current_version
    if name == 'mutex_len':
        if cv[0] ==  1 and cv[1] <= 1: return 32    
        if cv[0] ==  1 and cv[1] >= 2: return 64
        raise RuntimeError(f'no const for name = {name} and version = {cv[0]}.{cv[1]}.X')
    raise RuntimeError(f'no const for name = {name}')

def crc8(data, init: int = 0) -> int:
    if not hasattr(crc8, "table"):
        crc8.table = [0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15,
                      0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d,
                      0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65,
                      0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
                      0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5,
                      0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
                      0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85,
                      0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
                      0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2,
                      0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
                      0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2,
                      0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
                      0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32,
                      0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
                      0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42,
                      0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
                      0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c,
                      0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
                      0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec,
                      0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
                      0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c,
                      0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
                      0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c,
                      0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
                      0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b,
                      0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63,
                      0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b,
                      0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
                      0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb,
                      0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83,
                      0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb,
                      0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3]

    if isinstance(data, str):
        raise TypeError("Unicode-objects must be encoded before hashing")
    elif not isinstance(data, (bytes, bytearray)):
        raise TypeError("object supporting the buffer API required")

    for byte in data:
        init = crc8.table[init ^ byte]

    return init


format_regex = re.compile(r"(?<!%)(%%)*%"
                          + r"(?P<flags>#|0|\-|I|\'|\+)?"
                          + r"(?P<min_field_width>[0-9]+|\*)?"
                          + r"(?P<precision>\.([0-9]+|\*n)?)?"
                          + r"(?P<length>hh|h|l|ll|j|z|L)?"
                          + r"(?P<conversion_specifier>c|d|u|x|X|e|E|f|g|G|s|p|o|i)")


def build_new_format(match):
    conversion_specifier = match.group('conversion_specifier')
    min_field_width = match.group('min_field_width')
    precision = match.group('precision')
    flags = match.group('flags')
    format = ""
    if conversion_specifier == 'p':
        format = f"0x%" + \
            f"{flags if flags is not None else '0'}" + \
            f"{min_field_width if min_field_width is not None else '0'}" + \
            f"{precision if precision is not None else '.0'}" + \
            "x"
    else:
        format = f"%" + \
            f"{flags if flags is not None else ''}" + \
            f"{min_field_width  if min_field_width is not None else ''}" + \
            f"{precision  if precision is not None else ''}" + \
            f"{conversion_specifier}"
    return format


def clean_up_format(format: str) -> str:
    format = format_regex.sub(build_new_format, format)
    format = re.sub(r'\\n', ' ', format)
    return format



def decode(b: bytes) -> str:
    def validate_charsinformat(ch: str) -> str:
        if unicodedata.category(ch)[0] == "C" and ch != '\0':
            if ch == '\n':
                return '\\n'
            elif ch == '\t':
                return '\\t'
            elif ch == '\r':
                return '\\r'
            else:
                return f'\\x{ord(ch):02x}'
        return ch
    if isinstance(b, (bytes, bytearray)):
        data = b.decode(encoding="utf8", errors="ignore")
        data = ''.join(validate_charsinformat(ch) for ch in data)
        return data
    elif isinstance(b, str):
        return b
    else:
        raise RuntimeError("")


def get(raw: bytes, offset, size=None) -> bytes:
    if size is None:
        return raw[offset:]
    else:
        assert len(raw) >= (offset+size), f"{len(raw)} >= {(offset+size)}"
        return raw[offset: offset+size]


def get_int(raw, offset, size, signed=False):
    return int.from_bytes(get(raw, offset, size), current_Endian.value, signed=signed)


def get_float(raw, offset):
    return struct.unpack('f', get(raw, offset, 4))[0]


def get_double(raw, offset):
    return struct.unpack('d', get(raw, offset, 8))[0]


class Definition:
    size: int
    data: bytes()

    def __init__(self, file_offset: int, raw: bytes):
        self.size = int.from_bytes(raw[file_offset: file_offset+8], current_Endian.value)
        self.data = raw[file_offset+8:file_offset+8+self.size]

def timestamp_to_time(timestamp_ns) -> str:
    sub_seconds = (timestamp_ns % int(1e9))
    assert 0 <= sub_seconds < 1e9
    without_sub_seconds = (timestamp_ns // int(1e9))
    dt = datetime.fromtimestamp(without_sub_seconds, tz=pytz.timezone("UTC"))
    s = dt.strftime('%Y-%m-%d %H:%M:%S') + f".{sub_seconds :09d}"
    return s

def timestamp_to_str(timestamp_ns) -> str:
    return f"{Decimal(timestamp_ns) / Decimal(int(1e9)) : 021.9f}"

class Ringbuffer_Entry:
    location_in_file: int
    body_size: int
    body: bytes
    offset: int
    pid: int
    tid: int
    timestamp_ns: int
    time: int
    args_raw: bytes

    def __init__(self, body_size: int, body: bytes, location_in_file):
        self.location_in_file = location_in_file
        self.body_size = body_size
        self.body = body
        self.offset = get_int(body, 0, 6)
        self.pid = get_int(body, 6, 4)
        self.tid = get_int(body, 10, 4)

        nano_seconds = get_int(body, 14, 8)  # as nano seconds
        self.timestamp_ns = nano_seconds
        self.time = timestamp_to_time(self.timestamp_ns)
        self.args_raw = body[22:]
        assert self.body_size == len(body)


class Ringbuffer:
    raw: bytes
    file_offset: int
    dropped: int
    wrapped: int
    next_free: int
    last_valid: int
    entries: list
    used: int

    def __init__(self, file_offset: int, raw: bytes, recover_all:bool=False):
        mutex_len = get_const('mutex_len')
        logging.debug(f"ringbuffer load start (in file offset 0x{file_offset:x})")
        self.raw = raw
        self.file_offset = file_offset
        self.version = get_int(raw, file_offset + 0, 8)
        mutex = get(raw, file_offset + 8, mutex_len)  # mutex
        mutex_locked = any(mutex)
        logging.debug(f'mutex ({"locked" if mutex_locked else "unlocked"})')
        self.body_size = get_int(raw, file_offset + mutex_len + 8, 8)
        self.wrapped = get_int(raw, file_offset + mutex_len + 16, 8)
        self.dropped = get_int(raw, file_offset + mutex_len + 24, 8)
        header_entries_count = get_int(raw, file_offset + mutex_len + 32, 8)  # entries
        self.next_free = get_int(raw, file_offset + mutex_len + 40, 8)
        self.last_valid = get_int(raw, file_offset + mutex_len + 48, 8)
        reserved = get(raw, file_offset + mutex_len + 56, 40)  # reserved for future usage
        if any(e != 0 for e in reserved):
            logging.warning('reserved for future usage in ringbuffer is not zero')
        ringbody_fileoff = (file_offset + mutex_len + 56 + 40)
        logging.debug(f'next_free in file = 0x{ringbody_fileoff + self.next_free:X} last_valid in file = 0x{ringbody_fileoff + self.last_valid:X}')
        
        if self.last_valid <= self.next_free:
            self.used = self.next_free - self.last_valid
        else:
            self.used = (self.next_free + self.body_size) - self.last_valid
        assert 0 <= self.used

        self.entries = []
        
        if header_entries_count != 0:
            body_offset = mutex_len + 96
            body = get(raw, file_offset + body_offset, self.body_size)
            assert len(body) == self.body_size

            last_valid = self.last_valid

            def offset(base: int, value: int, max: int) -> int:
                base += value
                if base >= max:
                    base -= max
                return base
                        
            start_positions = set()
            ringbuffer_valid = not mutex_locked
            while (last_valid != self.next_free) or recover_all:
                if last_valid in start_positions:
                    break
                else:
                    start_positions.add(last_valid)
                    
                entry_head = bytearray(4)
                entry_head[0] = body[offset(last_valid, 0, self.body_size)]
                if entry_head[0] != ord('~'):
                    if ringbuffer_valid:
                        logging.warning(f"missing ringbuffer entry at file offset 0x{ringbody_fileoff + last_valid :X}")
                        ringbuffer_valid = False
                    last_valid = offset(last_valid, 1, self.body_size)
                    continue

                entry_head[1] = body[offset(last_valid, 1, self.body_size)]
                entry_head[2] = body[offset(last_valid, 2, self.body_size)]
                entry_head[3] = body[offset(last_valid, 3, self.body_size)]
                crc = crc8(entry_head)
                if crc != 0:
                    if ringbuffer_valid:
                        logging.warning(f"missing ringbuffer entry at file offset 0x{ringbody_fileoff + last_valid :X}")
                        ringbuffer_valid = False
                    last_valid = offset(last_valid, 1, self.body_size)
                    continue
                
                tmp_last_valid = offset(last_valid, 4, self.body_size)
                entry_body_size = entry_head[1:3]
                entry_body_size = int.from_bytes(entry_body_size, current_Endian.value) + 1
                entry_body = bytearray(entry_body_size)

                till_wrapped = self.body_size - tmp_last_valid

                first_part_size = min(entry_body_size, till_wrapped)
                entry_body[:first_part_size] = get(body, tmp_last_valid, first_part_size)

                second_part_size = max(entry_body_size - first_part_size, 0)
                entry_body[first_part_size:] = get(body, 0, second_part_size)
                crc = crc8(entry_body)
                if crc != 0:
                    if ringbuffer_valid:
                        logging.warning(f"missing ringbuffer entry at file offset 0x{ringbody_fileoff + last_valid :X}")
                        ringbuffer_valid = False
                    last_valid = offset(last_valid, 1, self.body_size)
                    continue
                ringbuffer_valid = True
                new_last_valid = offset(last_valid, 4 + entry_body_size, self.body_size)
                logging.debug(f"found ringbuffer entry from 0x{ringbody_fileoff+last_valid:X} to 0x{ringbody_fileoff+new_last_valid-1:X}")

                # remove crc
                entry_body = entry_body[:-1]
                entry_body_size = entry_body_size - 1
                
                self.entries.append(Ringbuffer_Entry(
                    entry_body_size, entry_body, ringbody_fileoff + last_valid))
                last_valid = new_last_valid
                pass
        
        missing_tracepoint_cound = max(0, header_entries_count - (len(self.entries) + self.dropped + mutex_locked))
        if missing_tracepoint_cound != 0:
            logging.warning(f"missing {missing_tracepoint_cound} entries in ringbuffer")
        logging.debug("ringbuffer time sort entry")
        # self.entries.sort(key = lambda e: e.timestamp_ns)
        logging.debug("ringbuffer load done")
        return


class Stack_Entry:
    hash: bytes
    body: bytes
    body_size : int
    file_offset: int
    begin : int
    end : int

    def __init__(self, file_offset: int, raw: bytes):
        head_crc = crc8(get(raw, file_offset, 29))
        assert head_crc == 0, f"head crc of stack entry invalid {head_crc}"
        
        self.file_offset = file_offset + 29
        self.hash = get(raw, file_offset, 16)
        _ = get(raw, file_offset + 16, 8)  # reserved for future usage
        self.body_size = get_int(raw, file_offset + 24, 4)
        _ = get(raw, file_offset + 28, 1) # crc
        self.body = get(raw, file_offset + 29, self.body_size)
        
        test_md5 = md5()
        test_md5.update(get(raw, file_offset + 24, 4))
        test_md5.update(self.body)
        test_md5 = test_md5.digest()
        assert test_md5 == self.hash, "invalid md5 in stack entry"
        del test_md5
        
        self.begin = self.file_offset
        self.end   = self.file_offset + self.body_size
        pass
    
    def get_total_size(self):
        return 29 + self.body_size

class Stack:
    entries: list

    def __init__(self, file_offset: int, raw: bytes):
        mutex_len = get_const('mutex_len')
        self.file_offset = file_offset
        self.version = get_int(raw, file_offset + 0, 8)
        assert self.version == 1
        _ = get(raw, file_offset + 8, mutex_len)  # stack mutex
        reserved = get(raw, file_offset + mutex_len + 8, 40)  # reserved for future usage
        if any(e != 0 for e in reserved):
            logging.warning('reserved for future usage in stack is not zero')
        stack_body_size = get_int(raw, file_offset + mutex_len + 48, 8)
        assert stack_body_size <= (len(raw)-file_offset), "stack_size must be false"

        self.entries = []
        offset = 0
        file_offset += mutex_len + 56
        while offset < stack_body_size:
            entry = Stack_Entry(file_offset + offset, raw)
            self.entries.append(entry)
            offset += entry.get_total_size()
        pass

    def get_blob(self, offset) -> Stack_Entry:
        for entry in self.entries:
            if entry.begin <= offset < entry.end:
                return entry
        return None


class DynamicTraceentry:
    def __init__(self, entry: Ringbuffer_Entry):
        self.entry = entry
        body = entry.body[22:]
        file_length = body.find(0)
        self.file = decode(get(body, 0, file_length))
        self.line = get_int(body, file_length + 1, 8)
        self.formatted = decode(get(body, file_length + 8 + 1)[:-1])
        pass


class StaticTraceentry:
    def __init__(self, entry: Ringbuffer_Entry, meta: Stack_Entry):

        self.entry = entry
        self.meta = meta

        self.meta_in_file_offset = get_int(entry.body, 0, 6)
        self.metaentry_in_metablob = self.meta_in_file_offset - meta.file_offset
        
        assert 0 <= self.metaentry_in_metablob <= len(meta.body)
        magic = get(meta.body, self.metaentry_in_metablob, 1)
        assert magic == b'{', f"{magic} =/= '{{'"
        self.type = get_int(meta.body, self.metaentry_in_metablob + 5, 1)

        assert self.type in [MetaEntryType.Printf.value, MetaEntryType.Dump.value], f"invalid meta data," +\
            " currently only type 1 and 2 is supported" +\
            f"but got {self.type} at file_offset 0x{self.in_file:X}"

        self.meta_size = get_int(meta.body, self.metaentry_in_metablob + 1, 4)
        self.raw_meta = get(meta.body, self.metaentry_in_metablob, self.meta_size)

        self.line = get_int(meta.body, self.metaentry_in_metablob + 6, 4)

        # get argument informations
        self.argument_count = get_int(
            meta.body, self.metaentry_in_metablob + 10, 1)
        self.argument_type_array = get(meta.body, self.metaentry_in_metablob +
                                       11, self.argument_count + 1)
        assert self.argument_type_array[-1] == 0, f"{self.argument_type_array}"
        self.argument_type_array = [
            chr(self.argument_type_array[i]) for i in range(self.argument_count)]
        self.arg_types = [StaticTraceentry.arg_type_to_string(
            i) for i in self.argument_type_array]

        # get file and format string
        self.string = get(meta.body,
                          self.metaentry_in_metablob + 12 + self.argument_count,
                          self.meta_size - 12 - self.argument_count)
        self.string = decode(self.string)
        (self.file, self.format, _) = self.string.split('\x00')
        assert len(_) == 0

        # get arguments
        if self.type == MetaEntryType.Dump.value:
            self.format += " =(dump)= \"%s\""
        
        logging.debug(f"static tp type = {self.type} in file at 0x{entry.location_in_file:X}")
        logging.debug(f"static tp at {self.file}:{self.line}")
        logging.debug(f"static tp format = {self.format}")
        
        formats = [m.groupdict() for m in format_regex.finditer(self.format)]

        self.args = []
        
        try:
            offset = 0
            for i in range(self.argument_count):
                (offset, value) = StaticTraceentry.get_arg(
                    entry.args_raw, offset, self.arg_types[i], formats[i])
                self.args.append(value)

            assert self.argument_count == len(formats), \
                f"argument types are {self.arg_types} \n"\
                    + f"but formates {formats} for type {self.type} and format \"{self.format}\""\
                    + f" at {self.file}:{self.line}"
            

            # create formatted string
        except Exception as e:
            error_str = f'Extracting Argument failed "{self.format}" from {self.file}:{self.line} with {e.with_traceback(e.__traceback__)} in  {traceback.format_exc()}'
            logging.error(error_str)
            self.formatted = error_str
            return
        logging.debug(f"static tp args = {self.args}")
        
        try:
            self.formatted = ((clean_up_format(self.format) % tuple(self.args)) if self.argument_count else self.format)
        except Exception as e:
            error_str = f'formating failed for  "{self.format}" from {self.file}:{self.line} with args ['
            for arg_type, arg_value in zip(self.arg_types, self.args):
                error_str += f'{arg_type}: "{arg_value}", '
            error_str += ']'
            logging.error(error_str)
            self.formatted = error_str
        return

    type_mapping = {
        'c': "uint8",
        'C': "int8",
        'w': "uint16",
        'W': "int16",
        'i': "uint32",
        'I': "int32",
        'l': "uint64",
        'L': "int64",
        'q': "uint128",
        'Q': "int128",
        'f': "float",
        'd': "double",
        's': "char*",
        'x': "<dump>",
        'p': "pointer",
    }

    @staticmethod
    def arg_type_to_string(argument_type_array: str) -> str:
        assert isinstance(argument_type_array, str), f"{type(argument_type_array) }"
        assert len(argument_type_array) == 1, f"{len(argument_type_array) }"
        return StaticTraceentry.type_mapping.get(argument_type_array, f'unknown type "{argument_type_array}"')
    
    @staticmethod
    def get_arg(raw: bytes, offset: int, t: int, format: dict):
        value = None
        if t in ["uint8"]:
            value = get_int(raw, offset, 1)
            offset += 1
        elif t in ["int8"]:
            value = get_int(raw, offset, 1, True)
            offset += 1
        elif t in ["uint16"]:
            value = get_int(raw, offset, 2)
            offset += 2
        elif t in ["int16"]:
            value = get_int(raw, offset, 2, True)
            offset += 2
        elif t in ["uint32"]:
            value = get_int(raw, offset, 4)
            offset += 4
        elif t in ["int32"]:
            value = get_int(raw, offset, 4, True)
            offset += 4
        elif t == "uint64" or t in ["pointer"]:
            value = get_int(raw, offset, 8)
            offset += 8
        elif t in ["int64"]:
            value = get_int(raw, offset, 8, True)
            offset += 8
        elif t in ["uint128"]:
            value = get_int(raw, offset, 16)
            offset += 16
        elif t in ["int128"]:
            value = get_int(raw, offset, 16, True)
            offset += 16
        elif t in ["float"]:
            value = get_float(raw, offset)
            offset += 4
        elif t in ["double"]:
            value = get_double(raw, offset)
            offset += 8
        elif t in ["char*"]:
            if format["conversion_specifier"] == 's':
                string_length = get_int(raw, offset, 4)
                offset += 4
                value = decode(get(raw, offset, string_length))
                if len(value) and value[-1] == '\0': value = value[:-1]
                offset += string_length
            elif format["conversion_specifier"] == 'p':
                value = get_int(raw, offset, 8)
                offset += 8
            else:
                raise RuntimeError("char* but not s or p in format")
            
        elif t in ["<dump>"]:
            string_length = get_int(raw, offset, 4)
            offset += 4
            value = get(raw, offset, string_length)
            value = " ".join(f"{x:02X}" for x in value)
            offset += string_length
        else:
            assert 0, "type unkown"

        return (offset, value)

    pass
def swap_odd_bytes(data: bytes) -> bytes:
    # Extract bytes at even and odd indices
    even_bytes = data[::2]
    odd_bytes = data[1::2]

    # Pair each odd byte with the next byte (even index)
    swapped_pairs = zip(odd_bytes, even_bytes)

    # Flatten the list of swapped byte pairs and convert it to bytes
    result = bytes(item for pair in swapped_pairs for item in pair)

    # If the original data has an odd length, append the last byte to the result
    if len(data) % 2 == 1:
        result += data[-1:]

    return result

class Tracebuffer:
    raw_file_data: bytes

    def __init__(self, raw: bytes, recover_all:bool=False):
        logging.debug("create new Tracebuffer")
        
        if raw[0:16] == b'#?~$rtcabefuef\0r':
            raw = swap_odd_bytes(raw)
            
        file_magic = get(raw, 0, 16)
        endians =  {b'?#$~tracebuffer\0':Endian.Little, b'cart~$#?'+b'\0reffube':Endian.Big}
        
        assert file_magic in endians , f"invalid file magic {decode(self.file_magic)}"
        global current_Endian
        current_Endian = endians[file_magic]
        
        file_head_crc = crc8(get(raw, 0, 56))
        assert file_head_crc == 0, f"file header crc invalid with 0x{file_head_crc:x}"
        self.file_magic = file_magic
        self.raw_file_data = raw
        self.file_size = len(raw)
        file_version = get_int(raw, 2*8, 8)
        version_major = 0xFF & (file_version // 0x10000)
        version_minor = 0xFF & (file_version // 0x100)
        version_patch = 0xFF & (file_version // 0x1)
        assert version_major == 1, f"invalid major version {version_major}"
        assert version_minor <= 2, f"invalid minor version {version_minor}"
        self.version = f"{version_major}.{version_minor}.{version_patch}"
        
        global current_version
        current_version = (version_major, version_minor)

        logging.debug("Tracebuffer: get definition")
        definition_offset = get_int(raw, 3*8, 8)
        self.definition = Definition(definition_offset, raw)

        logging.debug("Tracebuffer: get ringbuffer")
        ringbuffer_offset = get_int(raw, 4*8, 8)
        self.ringbuffer = Ringbuffer(ringbuffer_offset, raw, recover_all=recover_all)

        logging.debug("Tracebuffer: get stack")
        stack_offset = get_int(raw, 5*8, 8)
        self.stack = Stack(stack_offset, raw)
        logging.debug("Tracebuffer: done")

        self.entries = []
        entry : Ringbuffer_Entry
        for entry in self.ringbuffer.entries:
            in_file = entry.offset
            if in_file == 0:
                logging.warning(f'damaged tracebuffer entry which was fully valid with crc but not valid file offset')
                continue
            elif in_file == 1:
                entry = DynamicTraceentry(entry)
            else:
                stack_entry = self.stack.get_blob(in_file)
                magic = get(raw, in_file, 1)
                if magic == b'{':
                    entry = StaticTraceentry(entry, stack_entry)
                else:
                    logging.error(f"invalid meta data magic. At expected offset 0x{in_file:x}. Ringbuffer entry dropped")
                    continue
            
            if all(character.isspace() for character in entry.formatted):
                logging.info("empty trace entry skiped")
            self.entries.append(entry)
        pass

    def to_csv(self):
        csv = []
        entry: StaticTraceentry
        buffer_name = decode(self.definition.data)
        infos = [   
                    f"{{\"tracebuffer info version\":\"{tb.version}\"}}",
                    f"{{\"tracebuffer info size\":{tb.ringbuffer.body_size}}}",
                    f"{{\"tracebuffer info entries\":{len(tb.entries)}}}",
                    f"{{\"tracebuffer info in_use\":{tb.ringbuffer.used}}}",
                    f"{{\"tracebuffer info dropped\":{tb.ringbuffer.dropped}}}",
                    f"{{\"tracebuffer info wrapped\":{tb.ringbuffer.wrapped}}}",
                    f"{{\"tracebuffer info path\":\"{buffer_name}\"}}",
        ]
        for info in infos:
            csv.append({
                    "timestamp": timestamp_to_str(0),
                    "time": timestamp_to_time(0),
                    "tracebuffer": buffer_name,
                    "pid": -1,
                    "tid": -1,
                    "formatted": info,
                    "file": "clltk_decoder.py",
                    "line": 0,
                })
        for entry in self.entries:
            csv.append({
                "timestamp": timestamp_to_str(entry.entry.timestamp_ns),
                "time": entry.entry.time,
                "tracebuffer": buffer_name,
                "pid": entry.entry.pid,
                "tid": entry.entry.tid,
                "formatted": entry.formatted,
                "file": entry.file,
                "line": entry.line,
            })
        return csv

    pass

def genTracebuffer(folder_name, file_path:str, recover) -> Tracebuffer:
        try:
            file_path = os.path.abspath(file_path)
            with open(file_path, "rb") as file:
                raw_file_data = file.read()
                tb = Tracebuffer(raw_file_data, recover)
                logging.info(
                    f"tracebuffer: {decode(tb.definition.data):10s} "
                    f"size:{tb.ringbuffer.body_size:<6} "
                    f"entries:{len(tb.entries):<6} "
                    f"in_use:{tb.ringbuffer.used:<6} "
                    f"dropped:{tb.ringbuffer.dropped:<6} "
                    f"wrapped:{tb.ringbuffer.wrapped:<6} "
                    f"path:{folder_name} "
                )
                return tb
        except Exception as e:
            logging.error(f"failed to decode {file_path}, with {e} and {traceback.format_exc()}")
            global exit_code
            exit_code = 1
            pass
        return None

if __name__ == "__main__":
    import argparse
    import os
    import csv
    import pathlib
    import traceback
    import tempfile

    exit_code = 0

    def is_tracebuffer_file_name(name: str) -> bool:
        return (name.endswith(".cltk_trace")
                or name.endswith(".cltk_ktrace")
                or name.endswith(".clltk_trace")
                or name.endswith(".clltk_ktrace"))
        
    def is_archive(path: str) -> bool:
        return os.path.isfile(path) and tarfile.is_tarfile(path)

    def path_check(path: str):
        if not os.path.exists(path):
            raise argparse.ArgumentTypeError(
                f"\"{path}\" is not a valid path")

        if os.path.isdir(path):
            return path
        elif os.path.isfile(path) and is_tracebuffer_file_name(path):
            return path
        elif is_archive(path):
            return path
        else:
            raise argparse.ArgumentTypeError(f"{path} is not a valid")

    parser = argparse.ArgumentParser()

    parser.add_argument(
        "input",
        metavar="input",
        nargs='+',
        type=path_check,
        help="tracebuffer file with `.clltk_trace` or `clltk_ktrace` ending or " +
        "a folder where this files are recursively searched")

    parser.add_argument(
        "-o", "--output",
        metavar="output_file",
        help="output file",
        type=argparse.FileType("w", encoding="utf-8"),
        default="./output.txt")
        
    parser.add_argument('--log', type=str, help='log file path')


    parser.add_argument('-v', '--verbose',
                        action='store_true',
                        )
    parser.add_argument('-s', '--silent',
                        action='store_true',
                        )
    parser.add_argument('--recover',
                        action='store_true',
                        )
    parser.add_argument('--single-thread',
                        action='store_true',
                        )
    parser.usage = parser.format_help()

    args = parser.parse_args()

    if args.silent:
        logging.disable(logging.CRITICAL) 

    root = logging.getLogger()
    root.setLevel(logging.DEBUG)
    handler = logging.StreamHandler(sys.stdout)
    handler.setLevel(logging.DEBUG if args.verbose else logging.INFO)
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s', datefmt="%Y-%m-%d %H:%M:%S")
    handler.setFormatter(formatter)
    logging.getLogger().addHandler(handler)

    files = {input: input for input in args.input if (os.path.isfile(input)
             and is_tracebuffer_file_name(input))}

    folders = {input: input for input in args.input if os.path.isdir(input)}

    logging.debug("unpack archives to temporary directories")
    archives = [input for input in args.input if is_archive(input)]
    tmpdirs = []
    for input in archives:
        tmpdir = tempfile.TemporaryDirectory()
        archive = tarfile.open(input)
        estimated_unpacked_size = 0
        for member in archive.getmembers():
            estimated_unpacked_size += member.size
        if estimated_unpacked_size < 256*1024*1024:
            if sys.version_info.major == 3 and sys.version_info.minor >= 12:
                archive.extractall(tmpdir.name, filter='data')
            else:
                archive.extractall(tmpdir.name)
        else:
            logging.error(f'could not unpack {input} as it would be {estimated_unpacked_size} bytes')
        tmpdirs.append(tmpdir)
        folders[input] = tmpdir.name

    csv_data = []
    for folder_name, folder in folders.items():
        for root, dirs, dir_files in os.walk(folder):
            for file in dir_files:
                abspath = os.path.join(root, file)
                if is_tracebuffer_file_name(file) and '~' not in file:
                    relpath = os.path.relpath(abspath, folder)
                    name = f"{relpath} in {folder_name}"
                    files[name] = abspath
                    continue
                elif file == "additional_tracepoints.json":
                    with open(abspath, "r") as json_file:
                        json_content = json.load(json_file)
                        assert isinstance(json_content, list), "json must be array of objects"
                        for json_object in json_content:
                            d = {"timestamp": 0.0, "time": "", "tracebuffer": file, "pid": -1, "tid": -1, "formatted": "", "file": "", "line": -1}
                            for key, value in json_object.items():
                                if key not in ["timestamp", "formatted"]:
                                    logging.error(f"unknown key {key} in file {file}")
                                    continue
                                elif key == "timestamp":
                                    d["time"] = timestamp_to_time(value)
                                    d["timestamp"] = timestamp_to_str(value)
                                else:
                                    d[key] = value
                            csv_data.append(d)
                        try:
                            print()
                        except Exception as e:
                            logging.error(e)
                            continue

                continue
            continue
        continue
    files = dict(sorted(files.items(),key=lambda x:x[1] ))

    tracebuffers = []
    max_workers = 1 if args.single_thread or args.verbose else os.cpu_count()
    with concurrent.futures.ProcessPoolExecutor(max_workers=max_workers) as executor:
        futures = [executor.submit(genTracebuffer, *data, args.recover) for data in files.items()]
        for future in concurrent.futures.as_completed(futures):
            try:
                result = future.result()
                tracebuffers.append(result)
            except Exception as e:
                print(f"Error generating object: {e}")

    for tb in tracebuffers:
        if tb is not None:
            csv_data += (tb.to_csv())
    
    header = ["timestamp", "time", "tracebuffer", "pid", "tid", "formatted", "file", "line"]

    tracepoints = [header]
    for d in csv_data:
        row = [d.get(key, None) for key in header]
        tracepoints.append(row)
    
    if args.output.name.endswith(".csv"):
        writer = csv.writer(args.output, quoting=csv.QUOTE_MINIMAL, quotechar="\"", delimiter=",", escapechar="\\")
        writer.writerows(tracepoints)
    else:
        tracepoints[0][0] = " !" + tracepoints[0][0] # add space + exclamation at the beginning of first line to be always on top after a sort 
        def format(index, value):
            if index == 0:
                if isinstance(value, str):
                    return f"{value:21s}"
                return f"{value:021.9f}"
            elif index == 1:
                return f"{value:29s}"
            elif index == 2:
                return f"{value:20s}"
            elif index in [3,4]:
                if isinstance(value, str):
                    return f"{value:5s}"
                return f"{value:5d}"
            else:
                return f"{value}"
        
        for line in tracepoints:
            line = " | ".join(format(index, value) for index, value in enumerate(line))
            args.output.write(line+"\n")

    logging.info(f"written to {pathlib.Path(args.output.name).absolute()}")
    logging.debug("clean up temporary directories")
    for tmpdir in tmpdirs:
        tmpdir.cleanup()

    logging.debug("decoder done")
    exit(exit_code)

# %%
