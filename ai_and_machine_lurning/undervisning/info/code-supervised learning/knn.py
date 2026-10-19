import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from sklearn.neighbors import NearestNeighbors

data = pd.read_csv("https://raw.githubusercontent.com/uiuc-cse/data-fa14/gh-pages/data/iris.csv")
# input data
df = data[["sepal_length", "sepal_width"]]

# scatterplot of inputs data
plt.scatter(df["sepal_length"], df["sepal_width"])
plt.show()

X = df.values # create arrays

knn = NearestNeighbors(n_neighbors = 2) # knn model
knn.fit(X)
distances, indexes = knn.kneighbors(X) # distances and indexes of k-neaighbors from model outputs
outlier_index = np.where(distances.mean(axis = 1) > 0.15) ## 0.15 is outlier threshold
outlier_values = df.iloc[outlier_index]
print(f"Number of Outliers is: {len(outlier_values)}")
print("Outliers are:")
print(outlier_values)


plt.scatter(df["sepal_length"], df["sepal_width"], color = "b", s = 65) # plot data
plt.scatter(outlier_values["sepal_length"], outlier_values["sepal_width"], color = "r") # plot outlier values
plt.show()

