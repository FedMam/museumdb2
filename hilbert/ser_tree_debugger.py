# A program to view SER-trees in a human-readable way

import sys
import os

def parse_mbr(data: bytes) -> tuple[int, int, int, int]:
  return int.from_bytes(data[:8], byteorder=BYTEORDER), \
         int.from_bytes(data[8:16], byteorder=BYTEORDER), \
         int.from_bytes(data[16:24], byteorder=BYTEORDER), \
         int.from_bytes(data[24:], byteorder=BYTEORDER)

def print_offset_hex(offset: int) -> str:
  return '0x' + hex(offset)[2:].rjust(8 * 2, '0')

def print_mbr(left: int, top: int, right: int, bottom: int) -> str:
  return f'[{left}-{right}, {top}|{bottom}]'

def print_block_handle(data: int) -> str:
  return f'BlockHandle[0x' + hex(data)[2:].rjust(BLOCK_HANDLE_SIZE * 2, '0') + ']'

if len(sys.argv) <= 1:
  print('Usage: ser_tree_debugger.py <file-path>', file=sys.stderr)
  exit(255)

BYTEORDER = 'little'
BLOCK_HANDLE_SIZE = 16

file_path = sys.argv[1]

file = open(file_path, 'rb')

FOOTER_SIZE = 8 + 8 * 4 + 8
file.seek(-FOOTER_SIZE, os.SEEK_END)
data_size = file.tell()

MAGIC_NUMBER = 0x98e13db6c9118546
footer = file.read(FOOTER_SIZE)

if len(footer) < FOOTER_SIZE:
  print('Corrupted SER-tree file (size too small)', file=sys.stderr)
  file.close()
  exit(1)

footer_root_offset = int.from_bytes(footer[:8], byteorder=BYTEORDER)
footer_mbr = parse_mbr(footer[8:40])
footer_magic_number = int.from_bytes(footer[40:], byteorder=BYTEORDER)

if footer_magic_number != MAGIC_NUMBER:
  print('Corrupted SER-tree file (incorrect magic number)', file=sys.stderr)
  file.close()
  exit(1)
if footer_root_offset >= data_size:
  print('Corrupted SER-tree file (root offset out of range)', file=sys.stderr)
  file.close()
  exit(1)

print('SER-tree file')
print('-------------')
print('Root: ' + print_offset_hex(footer_root_offset))
print('MBR: ' + print_mbr(*footer_mbr))
print('-------------')

file.seek(0, os.SEEK_SET)
while True:
  node_offset = file.tell()
  if node_offset >= data_size:
    break

  node_header = file.read(5)
  is_leaf = node_header[0] != 0
  num_children = int.from_bytes(node_header[1:], byteorder=BYTEORDER)

  print(f'Node {print_offset_hex(node_offset)}, {"leaf" if is_leaf else "non-leaf"}, {num_children} children')

  if is_leaf:
    for i in range(num_children):
      block_handle = int.from_bytes(file.read(BLOCK_HANDLE_SIZE), byteorder=BYTEORDER)
      mbr = parse_mbr(file.read(8 * 4))
      print(f'  Child: {print_block_handle(block_handle)}, MBR: {print_mbr(*mbr)}')
  else:
    for i in range(num_children):
      offset = int.from_bytes(file.read(8), byteorder=BYTEORDER)
      mbr = parse_mbr(file.read(8 * 4))
      print(f'  Child: {print_offset_hex(offset)}, MBR: {print_mbr(*mbr)}')

file.close()
