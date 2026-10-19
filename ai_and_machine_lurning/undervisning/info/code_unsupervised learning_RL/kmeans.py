import matplotlib.pyplot as plt
from sklearn.cluster import KMeans


x1 = [4, 5, 10, 4, 3, 11, 14, 6, 10, 12] # define the x1-axis
x2 = [21, 19, 24, 17, 16, 25, 24, 22, 21, 21] # define the x2-axis

plt.scatter(x1, x2) # visualize the data in a scatter plot
plt.show() # show the plot

points = list(zip(x1, x2)) # create the set of points from the x1 and the x2 values

inertias = []
for i in range(1, 11): # vary the number of clusters and who the elbow score
    kmeans = KMeans(n_clusters=i)
    kmeans.fit(points)
    inertias.append(kmeans.inertia_)

plt.plot(range(1, 11), inertias, marker='o') # visualize the elbow score to determine the optimal number of clusters
plt.title('Elbow method')
plt.xlabel('Number of clusters')
plt.ylabel('Inertia')
plt.show()

kmeans = KMeans(n_clusters=2) # apply kmeans algorithm with 2 clusters
kmeans.fit(points)


plt.scatter(x1, x2, c=kmeans.labels_) # show the results of the clustering into a scatter plot
plt.show()

for i in range(len(points)):
    print(f"points:{points[i]}, cluster: {kmeans.labels_[i]}")
