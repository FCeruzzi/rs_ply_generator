import cv2 as cv

path_rgb = './segnet/'
path_inferenza = './segnet/mask/'
path_output = './segnet/inference/'

#read image
def read_image(path):
	im = cv.imread(str(path))
	return cv.cvtColor(im, 1)

#write image
def write_image(path,image):
	cv.imwrite(str(path), image)

def main():
	img_inferenza = read_image(path_inferenza + 'image.png')
	img_rgb = read_image(path_rgb + 'image.png')

	black = [0,0,0]

	for x in range(0,512):
		for y in range(0,512):
			channels_xy = img_inferenza[y,x]
			#print(str(i) + str(img_inferenza[y,x]))
			if all(channels_xy == black):
			    img_rgb[y, x, 0] = 0
			    img_rgb[y, x, 1] = 0
			    img_rgb[y, x, 2] = 0
			#print('dopo' + str(img[y,x]))
			    
	print('save inference')
	write_image(path_output + 'image.png',img_rgb)

if __name__ == "__main__":
	main()
