#!/usr/bin/env python3

import random

random.seed(16552)

def main():
    print('''
    # Random single threaded test case
    ''')
    print(1)
    print('o')
    j = 0

    inst = random.choice(('GOOG', 'AMZN', 'AAPL'))
    command = random.choice(('B', 'S'))
    price = random.randint(1000, 9000)
    count = random.randint(1, 20)
    print(f'{command} {j} {inst} {price} {count}')
    j += 1

    for i in range(10000):
        inst = random.choice(('GOOG', 'AMZN', 'AAPL'))
        command = random.choice(('B', 'S', 'C'))
        if command == 'B' or command == 'S':
            price = random.randint(1000, 9000)
            count = random.randint(1, 20)
            print(f'{command} {j} {inst} {price} {count}')
            j += 1
        else:
            order_id = random.randint(0, j-1)
            print(f'{command} {order_id}')

    print('x')

if __name__ == '__main__':
    main()
