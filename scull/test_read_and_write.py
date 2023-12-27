import os
import argparse

scull_device = os.path.join(os.path.sep, "dev", "scull0")

def write_to_device(device=scull_device, bytes=7000):
    data_to_write = 'A' * bytes
    try:
        with open(device, "w") as file:
            ret = file.write(data_to_write)
            print(f"write returned with {ret}")
    except Exception as e:
        print(e)

def read_from_device(device=scull_device, bytes=5000):
    try:
        with open(device, "r") as file:
            ret = data = file.read(bytes)
            print(data)
            print(f"read returned with {ret}")
    except Exception as e:
        print(e)


def get_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument("option")

    return parser


if __name__ == "__main__":
    args = get_parser().parse_args()

    if args.option == "write":
        write_to_device()
    elif args.option == "read":
        read_from_device()
    elif args.option == "both":
        write_to_device()
        read_from_device()

    