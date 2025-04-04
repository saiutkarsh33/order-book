#!/usr/bin/env python3

import sys
import random

def main(num_thr, num_instr, ords_per_instr):
	print(f'''
	# tests normal buy/sell mechanics for up to {num_instr} instruments
	# using {num_thr} clients and up to {ords_per_instr} orders per instrument
	''')

	if ords_per_instr % num_thr != 0:
		print("ords_per_instr must be a multiple of num_thr")
		sys.exit(1)

	print(num_thr)
	print('o')

	oid = 0
	instr_list = [f"INSTR{i}" for i in range(num_instr)]
	random.shuffle(instr_list)

	for instr in instr_list:
		for i in range(int(ords_per_instr / num_thr)):
			for tid in range(num_thr):
				print(f"{tid} {'B' if tid % 2 == 0 else 'S'} {oid} {instr} 2700 10")
				oid += 1

	print('x')

if __name__ == '__main__':
	if len(sys.argv) < 4:
		print(f"usage: ./many-instruments.py <num_threads> <num_instrs> <ords_per_instr>")
		sys.exit(0)

	main(int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]))
