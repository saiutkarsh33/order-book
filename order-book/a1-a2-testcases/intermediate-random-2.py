#!/usr/bin/env python3

import random

random.seed(13782)

def main():
    print('''
    # Random doubly threaded test case
    ''')
    print(2)
    print('o')
    j = 0

    for tid in range(2):
        starting_j = j

        inst = random.choice(('GOOG', 'AMZN', 'AAPL'))
        command = random.choice(('B', 'S'))
        price = random.randint(1000, 9000)
        count = random.randint(1, 20)
        print(f'{tid} {command} {j} {inst} {price} {count}')
        j += 1

        for i in range(10000):
            inst = random.choice(('GOOG', 'AMZN', 'AAPL'))
            command = random.choice(('B', 'S', 'C'))
            if command == 'B' or command == 'S':
                price = random.randint(1000, 9000)
                count = random.randint(1, 20)
                print(f'{tid} {command} {j} {inst} {price} {count}')
                j += 1
            else:
                order_id = random.randint(starting_j, j-1)
                print(f'{tid} {command} {order_id}')

    print('x')

if __name__ == '__main__':
    main()
