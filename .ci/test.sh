#!/bin/bash

set -e

python3 -c "from PIL import Image"

python3 comparison.py
python3 quote_to_image.py 100
