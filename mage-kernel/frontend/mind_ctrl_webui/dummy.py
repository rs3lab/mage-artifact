#!/bin/usr/python3
# for i in range(25):
#     print(f"Dummy script: [{i}]")

# Write a python program that prints out start shape with *:

import time

print("Star shape with *: ")
for _ in range(3):
    for i in range(5):
        for j in range(5):
            if i == 2 or j == 2 or i + j == 4 or i - j == 2:
                print("*", end="", flush=True)
            else:
                print(" ", end="", flush=True)
            time.sleep(0.2)
        print()
    print("\n")
