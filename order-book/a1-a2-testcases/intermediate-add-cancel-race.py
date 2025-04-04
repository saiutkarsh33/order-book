#!/usr/bin/env python3

def main():
    print('''
    # Adds a pair of concurrent orders on the same side, cancels them immediately
    ''')
    print(2)
    print('o')
    j = 0
    for i in range(4000):
        print(f'0 B {j} GOOG 2700 1')
        print(f'0 C {j}')
        j += 1
        print(f'1 B {j} GOOG 2700 1')
        print(f'1 C {j}')
        j += 1
        print(f'0 w {j-1}')
        print(f'1 w {j-2}')
    print('x')

if __name__ == '__main__':
    main()
