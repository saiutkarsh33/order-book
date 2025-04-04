#!/usr/bin/env python3

def main():
    print('''
    # Adds a pair of concurrent orders on the same side, cancels them immediately
    ''')
    num_pairs = 20
    print(num_pairs * 2)
    print('o')
    j = 0
    for i in range(400):
        for tid in range(num_pairs):
            print(f'{tid} B {j} INST{tid} 2700 1')
            print(f'{tid} C {j}')
            j += 1
            print(f'{tid+num_pairs} B {j} INST{tid} 2700 1')
            print(f'{tid+num_pairs} C {j}')
            j += 1
            print(f'{tid} w {j-1}')
            print(f'{tid+num_pairs} w {j-2}')
            print(f'{tid} S {j} INST{tid} 2700 2')
            j += 1
            print(f'{tid},{tid+num_pairs} .')
    print('x')

if __name__ == '__main__':
    main()
