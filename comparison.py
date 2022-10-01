import time
start = time.time()
i = 0
while i < 100000000:
    i += 1
print("Comparison:", time.time() - start)
