import numpy as np
import matplotlib.pyplot as plt
import random

"""
Define a simple grid world
# 0 - empty space
# 1 - wall
# 2 - goal
"""
grid = [
    [0, 0, 0, 0, 2],
    [0, 1, 1, 0, 1],
    [0, 0, 0, 0, 0]
]

n_rows, n_cols = len(grid), len(grid[0])
actions = ['up', 'down', 'left', 'right']
n_actions = len(actions)

# Parameters
alpha = 0.1
gamma = 0.9
epsilon = 0.1
episodes = 1000

# Convert state to index
def state_index(row, col):
    return row * n_cols + col

# Check if move is valid
def is_valid_move(row, col):
    return 0 <= row < n_rows and 0 <= col < n_cols and grid[row][col] != 1

# Move in the grid
def step(row, col, action):
    if action == 'up':
        next_row, next_col = row - 1, col
    elif action == 'down':
        next_row, next_col = row + 1, col
    elif action == 'left':
        next_row, next_col = row, col - 1
    elif action == 'right':
        next_row, next_col = row, col + 1

    if is_valid_move(next_row, next_col):
        if grid[next_row][next_col] == 2:
            return next_row, next_col, 10, True
        else:
            return next_row, next_col, -0.1, False
    else:
        return row, col, -1, False  # Penalty for hitting wall or boundary

# Initialize Q-table
q_table = np.zeros((n_rows * n_cols, n_actions))
rewards = []

# Training loop
for ep in range(episodes):
    row, col = 0, 0  # Start position
    total_reward = 0
    done = False

    while not done:
        state = state_index(row, col)
        if random.uniform(0, 1) < epsilon:
            action_idx = random.randint(0, n_actions - 1)
        else:
            action_idx = np.argmax(q_table[state])

        action = actions[action_idx]
        next_row, next_col, reward, done = step(row, col, action)
        next_state = state_index(next_row, next_col)

        old_value = q_table[state, action_idx]
        next_max = np.max(q_table[next_state])

        q_table[state, action_idx] = old_value + alpha * (reward + gamma * next_max - old_value)

        row, col = next_row, next_col
        total_reward += reward

    rewards.append(total_reward)

# Plotting rewards
plt.plot(rewards)
plt.title("Total Rewards per Episode (Grid World)")
plt.xlabel("Episode")
plt.ylabel("Total Reward")
plt.grid(True)
plt.show()
