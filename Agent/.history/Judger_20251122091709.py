import config
import prompt
class Judger:
    def __init__(self,id):
        self.prompt = prompt.background_prompt + "\nYou are a supervisor in the coin-mixing system. Now you need to identify whether a user is suspected of money laundering. You will receive the user's historical transaction sequence and make your judgment based on it."

    def judge(self, sequence):
        count = 0
        res = '\n'.join(sequence)
        user_input = "The following is a user's transaction sequence: " + res + ". \nBased on the above sequence, determine whether the user is suspected of money laundering. If you think they are, output 1; if not, output 0. Don't output anything else."
        completion = config.client.chat.completions.create(
            model=config.model,
            messages=[
                {'role': 'system', 'content': self.prompt},
                {'role': 'user', 'content': user_input}
            ]
        )
        result = completion.choices[0].message.content
        if result == '1':
            return True
        else:
            return False