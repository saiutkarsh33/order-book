#!/usr/bin/env python3

import random

random.seed(23300)

def main():
    print('''
    # Random heavily threaded test case
    ''')
    print(40)
    print('o')
    j = 0

    for tid in range(40):
        starting_j = j

        inst = random.choice(('GOOG', 'AMZN', 'AAPL'))
        command = random.choice(('B', 'S'))
        price = random.randint(1000, 9000)
        count = random.randint(1, 20)
        print(f'{tid} {command} {j} {inst} {price} {count}')
        j += 1

        for i in range(1000):
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
