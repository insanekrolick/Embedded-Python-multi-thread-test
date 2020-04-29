import time
import random

print('External Python Program started...')
counter = 0
limit = random.randrange(3, 10, 1)
print(" Limit: " + str(limit))

while counter < limit:
	print('Python: counter = ', str(counter))
	counter += 1
	time.sleep(2)
print(': End of Py Program')

