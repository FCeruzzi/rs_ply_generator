import cv2
import numpy as np

from keras.preprocessing.image import img_to_array


def category_label(labels, dims, n_labels):
    x = np.zeros([dims[0], dims[1], n_labels])
    for i in range(dims[0]):
        for j in range(dims[1]):
            x[i, j, labels[i][j]] = 1
    x = x.reshape(dims[0] * dims[1], n_labels)
    return x

def test_data_generator(lists, dims, n_labels):
    for i in range(len(lists)):
        imgs = []
        labels = []
        line = lists[i]
        names = line.split(" ")
        # images
        original_img = cv2.imread(names[0].rstrip('\n'))[:, :, ::-1]
        new_size = (dims[0], dims[1])
        resized_img = cv2.resize(original_img, new_size)
        array_img = img_to_array(resized_img) / 255
        # masks
        original_mask = cv2.imread(names[1].rstrip('\n'))
        resized_mask = cv2.resize(original_mask, (dims[0], dims[1]))
        array_mask = category_label(resized_mask[:, :, 0], dims, n_labels)
        labels.append(array_mask)
        imgs.append(array_img)
        imgs = np.array(imgs)
        labels = np.array(labels)
        yield imgs, labels


def data_gen_small(lists, batch_size, dims, n_labels):
    while True:
        ix = np.random.choice(np.arange(len(lists)), batch_size)
        imgs = []
        labels = []
        for i in ix:
            line = lists[i]
            names = line.split(" ")
            original_img = cv2.imread(names[0].rstrip('\n'))[:, :, ::-1]
            new_size = (dims[0], dims[1])
            resized_img = cv2.resize(original_img, new_size)
            array_img = img_to_array(resized_img) / 255
            imgs.append(array_img)
            # masks
            original_mask = cv2.imread(names[1].rstrip('\n'))
            resized_mask = cv2.resize(original_mask, (dims[0], dims[1]))
            array_mask = category_label(resized_mask[:, :, 0], dims, n_labels) 
            labels.append(array_mask)
        imgs = np.array(imgs)
        labels = np.array(labels)
        yield imgs, labels
