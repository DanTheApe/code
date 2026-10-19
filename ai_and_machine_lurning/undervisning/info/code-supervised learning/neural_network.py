from sklearn.datasets import load_digits
from sklearn.model_selection import train_test_split
from sklearn.neural_network import MLPClassifier
from sklearn.metrics import accuracy_score

# Step 1
# Loading the dataset
dataset = load_digits()

# Step 2
# Splitting the data into train and test
# 80 - 20 Split
x_train, x_test, y_train, y_test = (train_test_split
                                    (dataset.data, dataset.target,
                                     test_size=0.20))

# Step 3
# Making the Neural Network Classifier
neural_network = MLPClassifier()

# Step 4
# Training the model on the training data and labels
neural_network.fit(x_train, y_train)

# Step 5
# Testing the model i.e. predicting the labels of the test data.
y_pred = neural_network.predict(x_test)

# Step 6
# Evaluating the results of the model
accuracy = accuracy_score(y_test, y_pred) * 100

# Step 7
# Printing the Results
print(f"Accuracy for Neural Network is: {accuracy}")
for element_1, element_2 in zip(y_test, y_pred):
       if element_1 != element_2:
           print(f"label:{element_1}, prediction:{element_2}")
