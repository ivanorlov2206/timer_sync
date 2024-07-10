#!/bin/python3

frame_rate = 8000
channels = 1
length = 4
sample_size = 2

pattern = b'ABCD'

data = pattern * (frame_rate * channels * length * sample_size // len(pattern))
f = open("sound.wav", "wb")
f.write(data)
f.close()
