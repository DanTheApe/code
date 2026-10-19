import argparse
import gymnasium as gym
import time


def build_arg_parser():
    parser = argparse.ArgumentParser(description='Run an environment')
    parser.add_argument('--input-env', dest='input_env', required=True,
            choices=['cartpole', 'mountaincar', 'pendulum', 'taxi', 'lake'], 
            help='Specify the name of the environment')
    return parser

if __name__=='__main__':
    args = build_arg_parser().parse_args()
    input_env = args.input_env

    name_map = {'cartpole': 'CartPole-v1', 
                'mountaincar': 'MountainCar-v0',
                'pendulum': 'Pendulum-v1',
                'taxi': 'Taxi-v3',
                'lake': 'FrozenLake-v0'}

    # Create the environment and reset it
    env = gym.make(name_map[input_env], render_mode='human')
    env.reset()
    

    # Iterate 100 times
    observation, info = env.reset()
    for _ in range(100):
        # Render the environment
        env.render()
        time.sleep(0.05)

        # take a random action
        # (in case one direction needed: 
        # 0: Push cart to the left, 1: Push cart to the right)
        action = env.action_space.sample() 
        observation, reward, terminated, truncated, info = env.step(action)
        
    env.close()