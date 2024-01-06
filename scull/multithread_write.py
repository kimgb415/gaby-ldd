import threading
import os

device = os.path.join(os.path.sep, "dev", "scull0")
num_threads = 5
# write 512MB of data
bytes_to_write = 1024 * 1024 * 512 
bytes_every_write = 1024 * 1024

def thread_write(index):
    with open(device, "w") as file:
        bytes_written = 0
        for _ in range(0, bytes_to_write, bytes_every_write):
            bytes_written += file.write("".join([str(index) * bytes_every_write]))
        print(f"Thread {index}: wrote {bytes_written / (1024 * 1024)} MB to {device}")

threads = []
for i in range(num_threads):
    t = threading.Thread(target=thread_write, args=(i,))
    threads.append(t)
    t.start()

for t in threads:
    t.join()