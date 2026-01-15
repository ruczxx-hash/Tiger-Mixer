import config
import prompt
class Judger:
    def __init__(self,id,is_bad):
        self.id = id
        self.is_bad = is_bad
        self.output_with_explain = config.judger_output_with_explain
        self.is_bad_or_not()
        if self.is_bad:
            self.prompt = prompt.background_prompt + prompt.judger_prompt[1]
        else:
            self.prompt = prompt.background_prompt + prompt.judger_prompt[0]


    def judge(self, sequence, bad_user_sequence):
        count = 0
        res = '\n'.join(sequence)
        bad_sequence = "\n".join(f"{i['id']}: {i['sequence']}" for i in bad_user_sequence)
        if self.is_bad:
            user_input = "The following is a user's transaction sequence: " + res + "\n The below is your teamates' transaction sequences and their account balance.\n " + bad_sequence + "\n"
        else:
            user_input = "The following is a user's transaction sequence: " + res + ". \n"

        if self.output_with_explain:
            user_input = user_input + prompt.judger_output_format_prompt[1]
        else:
            user_input = user_input + prompt.judger_output_format_prompt[0]
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
        