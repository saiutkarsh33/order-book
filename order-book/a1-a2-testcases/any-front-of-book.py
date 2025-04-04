#!/usr/bin/env python3

import sys
import random

def main(num_thr, num_instr, ords_per_instr):
	print(f'''
	# always insert orders to the front of the book, with {num_instr} instruments,
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

	price = 10000000

	for instr in instr_list:
		for i in range(ords_per_instr // num_thr):
			thr_list = [x for x in range(num_thr)]
			random.shuffle(thr_list)

			for tid in thr_list:
				assert price > 0
				print(f"{tid} S {oid} {instr} {price} {random.randint(69, 420)}")
				price -= random.randint(2, 6)
				oid += 1

	print('x')

if __name__ == '__main__':
	if len(sys.argv) < 4:
		print(f"usage: ./script.py <num_threads> <num_instrs> <ords_per_instr>")
		sys.exit(0)

	main(int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]))
