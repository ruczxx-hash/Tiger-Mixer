class Transactions:
    def __init__(self, time, send):
        self.time = time
        self.send = send
        self.failed = 0

    def update_failed(self):
        self.failed = self.failed + 1