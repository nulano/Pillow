import time

from PIL import features

start = time.time()
i = 0
while i < 100000000:
    i += 1
print("Comparison:", time.time() - start)

features.pilinfo(supported_formats=False)
