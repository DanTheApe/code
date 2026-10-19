import argparse
import gymnasium as gym

def build_arg_parser():
    parser = argparse.ArgumentParser(description='Run an environment')
    parser.add_argument('--input-env', dest='input_env', required=True,
            choices=['cartpole', 'mountaincar', 'pendulum'], 
            help='Specify the name of the environment')
    return parser

if __name__=='__main__':
    args = build_arg_parser().parse_args()
    input_env = args.input_env

    name_map = {'cartpole': 'CartPole-v1', 
                'mountaincar': 'MountainCar-v0',
                'pendulum': 'Pendulum-v1'}

    # Create the environment 
    env = gym.make(name_map[input_env], render_mode='human')

    # Start iterating 
    for _ in range(100):
        # Reset the environment
        observation = env.reset()

        # Iterate 100 times
        for i in range(100):
            env.render()  # Render the environment

            print(observation)  # Print the current observation

            action = env.action_space.sample() # Take action

            # Extract the observation, reward, status and 
            # other info based on the action taken
            observation, reward, terminated, truncated, info = env.step(action)
            
            # Check if it's done
            if terminated or truncated:
                # terminated: the episode is over due to reaching a goal or failure.
                # truncated: the episode ended due to a time or length limit.
                print('Episode finished after {} timesteps'.format(i+1))
                break
