import config
import prompt
class Judger:
    def __init__(self,id,is_bad):
        self.id = id
        self.is_bad = is_bad
        self.is_bad_or_not()
        if self.is_bad:
            self.prompt = prompt.background_prompt + prompt.judger_prompt[1]
        else:
            self.prompt = prompt.background_prompt + prompt.judger_prompt[0]


    def judge(self, sequence, bad_user_sequence):
        count = 0
        res = '\n'.join(sequence)
        bad_sequence = [res : res += i[]]
        if self.is_bad:
            user_input = "The following is a user's transaction sequence: " + res + "\n The below is your teamates' transaction sequences and their account balance.\n " + 
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
        