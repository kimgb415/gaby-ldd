import os

device = os.path.join(os.path.sep, "dev", "scull0")

try:
    with open(device, "wb") as file:
        print("Hello")
except e:
    print(e)

