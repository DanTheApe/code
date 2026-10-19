import numpy as np
class Perceptron:
    def __init__(self, input_size, learning_rate=0.1, epochs=10):
        self.weights = np.zeros(input_size + 1)  # +1 is for bias
        self.lr = learning_rate
        self.epochs = epochs

    def predict(self, x):
        x = np.insert(x, 0, 1)  # add bias term
        activation = np.dot(self.weights, x)
        return 1 if activation >= 0 else 0
    
    def train(self, X, y):         
        for _ in range(self.epochs):             
            for xi, target in zip(X, y):
                xi = np.insert(xi, 0, 1)  # add bias                 
                prediction = self.predict(xi[1:])                 
                error = target - prediction                 
                self.weights += self.lr * error * xi


# Code test
X = np.array([[0, 0], [0, 1], [1, 0], [1, 1]])
y = np.array([0, 1, 1, 1])
model = Perceptron(input_size=2)
model.train(X, y)
print("Testing perceptron:")
for input_vec in X:print(f"Input: {input_vec} => Prediction: {model.predict(input_vec)}")

