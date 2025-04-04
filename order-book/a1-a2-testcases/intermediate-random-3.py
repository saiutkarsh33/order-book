#!/usr/bin/env python3

import random

random.seed(1722)

def main():
    print('''
    # Random doubly threaded test case, periodic full clean to keep order book small
    ''')
    print(2)
    print('o')
    j = 0

    for epoch in range(100):
        orders_by_tid = []
        for tid in range(2):
            starting_j = j
            orders = []

            for i in range(100):
                inst = random.choice(('GOOG', 'AMZN', 'AAPL'))
                if orders:
                    command = random.choice(('B', 'S', 'C'))
                else:
                    command = random.choice(('B', 'S'))
                if command == 'B' or command == 'S':
                    price = random.randint(1000, 9000)
                    count = random.randint(1, 20)
                    print(f'{tid} {command} {j} {inst} {price} {count}')
                    orders.append(j)
                    j += 1
                else:
                    order_id = random.randint(starting_j, j-1)
                    try:
                        orders.remove(order_id)
                    except:
                        pass
                    print(f'{tid} {command} {order_id}')

            orders_by_tid.append(orders)

        # at end of epoch, wait then cancel all orders
        print('.')

        for tid, orders in enumerate(orders_by_tid):
            for order_id in orders:
                print(f'{tid} C {order_id}')

    print('x')

if __name__ == '__main__':
    main()
