file = open("tmp.fifo", "rb", buffering=0)
isValid = True

while isValid :
	var = file.read(65536 * 10)
	if var == b'' :
		isValid = False
		continue
	else :
		size = len(var)
		ret = '%d bytes' % size
		if size >= 2 :
			ret = ret + ' : %02x%02x' % (var[0], var[1])
			if var[0] != 170 :
				ret = ret + ' -> ERROR'
		else :
			ret = ret + ' -> ERROR'
		print(ret)







