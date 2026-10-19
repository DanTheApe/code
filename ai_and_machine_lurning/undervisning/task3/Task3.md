The aim of this assignment is to tmplement and test the K-Means clustering algorithm from scratch to understand how it works internally.

TASK:

Implement the K-Means algorithm from scratch
Do not use library implementations of the core algorithm (e.g., sklearn.cluster.KMeans) — you may use libraries like NumPy for numerical operations. Your implementation must include:

Random initialization of centroids. 

Assignment of each data point to its nearest centroid

Update of centroid positions based on assigned points

A stopping criterion (e.g.,centroid movement below a threshold, or a maximum number of iterations). 

2. Apply your implementation to the Iris dataset
Run your K-Means implementation on the Iris dataset (use only the feature columns, not the class labels, for clustering).

3. Test multiple values of k
Run your algorithm with at least three different values of k (e.g., k = 2, 3, 4, 5). For each value of k:

Visualize the resulting clusters (e.g., scatter plot using two features or a dimensionality reduction technique such as PCA)

4. Discuss the effect of k

In a short written discussion, address:

How does the choice of k affect the clustering results?
Which value of k seems most appropriate for the Iris dataset, and why?
How does this relate to the known number of classes in the Iris dataset?
