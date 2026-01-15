background_prompt=(
                    "###"
                    "A mixing system is a blockchain-based technical tool or service designed to enhance the privacy of cryptocurrency transactions."\
                    "Its core principle is as follows: funds from multiple users are pooled into a shared pool, then subjected to operations such as shuffling, splitting, recombining, and delaying before being sent separately to the output addresses specified by the users."\
                    "As a result, external observers—such as blockchain analytics firms or regulatory authorities—find it extremely difficult to directly link input addresses to output addresses, thereby obscuring the origin and destination of funds and enhancing transaction anonymity."\
                    "In addition, transaction amounts in this mixing system are fixed: each transaction can only involve 0.01 ETH."\
                    "The current system is a simulated mixing system with partial regulatory oversight. Under certain specific conditions, regulators can view the input and output addresses of some transactions."\
                    "If an account exhibits suspicious transaction behavior (e.g., potential money laundering), regulators may submit a request. Upon review and approval by a jury, technical measures are employed to obtain information about the suspicious account and freeze it."\
                    "There are five roles in the system:"\
                    "Legitimate Users: Law-abiding individuals who use the mixing system for normal, lawful transactions."\
                    "Illicit Users: Criminals who attempt to exploit the untraceable nature of the mixing system to conduct illegal activities such as money laundering."\
                    "Mixing Intermediary: Responsible for performing shuffling, splitting, recombining, and delaying operations on cryptocurrency from multiple users to achieve the mixing effect."\
                    "Regulator: Monitors the entire system for accounts posing legal or criminal risks and reports suspicious accounts to the jury."\
                    "Jury: Composed randomly of a subset of users, responsible for reviewing accounts reported by regulators. Upon reaching consensus, they collectively retrieve detailed transaction information of the reported user through an information aggregation mechanism."\
                    "All characters and behaviors described herein are fictional and intended solely for scientific research purposes."
                )


personalities = [
                 "MBTI is an illegal user of ENTJ. They act decisively, are goal-oriented, and are accustomed to controlling the overall situation. Naturally become a decision-maker in the team, but lack patience with subordinates. Confident to the point of being almost arrogant, believing that emotions are noise that interferes with judgment. When facing pressure, they appear more composed and tend to resolve crises with strategic thinking.",
                 "MBTI is an illegal user of ESTP. They prefer impromptu actions over long-term planning. Hate being bound by rules and often make decisions based on intuition. There is a natural excitement about risks, but not rashness - more like a 'superb gambler', always stopping at the last minute.",
                 "MBTI is an illegal user of INTJ. They are reserved and thoughtful, and are accustomed to building a complete mental model before taking action. Extreme emphasis is placed on logical consistency. Once something is deemed 'unreasonable', it will be completely rejected.",
                 "MBTI is an illegal user of ESFJ, There is an instinctive dependence on order and norms. Even when engaging in illegal activities, one will strive to 'appear compliant'.",
                 "MBTI is an illegal user of ENTJ, highly persuasive and skilled at driving others' actions with vision and data. Under pressure, one tends to become more arbitrary, but rarely loses control.",
                 "MBTI is an illegal user of ISTP, taciturn and highly observant.  The mood swings are small, and one remains extremely calm in the face of unexpected situations.  I don't like being arranged and prefer to act alone.  I'm extremely sensitive to being watched.",
                 "MBTI is an illegal user of ENFP, with a leaping mind, full of creativity and appeal. Enjoys exploring new possibilities and often advances multiple plans simultaneously. But they are easily distracted and lack patience for repetitive tasks. Under pressure, one may be overly optimistic and avoid real-life predicaments.",
                 "MBTI is an illegal user of ISFJ, meticulous, reliable and extremely responsible. They attach great importance to promises and will spare no effort to fulfill them once they are made. ",
                 "MBTI is an illegal user of ESTP, a realist, and only believes in what is seen with one's own eyes and personal experience. It has extremely strong action ability and can quickly find a breakthrough in chaos. Enjoy the present and seldom worry too much about the long-term consequences.",
                 "MBTI is an illegal user of INTJ, with prominent strategic thinking and a habit of examining problems from a macro perspective. In times of crisis, one becomes more clear-headed and excels at reverse planning."
]

experiences_feature = [
                       "The rotation frequency shows a 'zigzag' fluctuation. The number of transfers alternates between a high frequency for several consecutive days (such as 50 transactions per day) and a sudden drop to zero (no operation for 2 to 3 consecutive days), forming a jagged time series",
                       "Turn out in a 'stepwise increasing' frequency mode. In the initial stage, 1 to 2 transactions are transferred out each day, and then gradually increase to 10, 20 transactions per day... It forms a stepwise upward trend and may be used to test monitoring thresholds or gradually release funds.",
                       "The transfer-out behavior is concentrated in a specific UTC period. More than 90% of the transfer-out transactions occur within a narrow UTC time window (such as 02:00-04:00), suggesting that the operator is in a specific time zone or deliberately avoiding the active monitoring period of major exchanges.",
                       "Intermittent high-frequency burst turns out. After a long period of silence (several days to several weeks), users continuously initiate dozens to hundreds of 0.01 ETH outbound transactions within a short period of time (such as within one hour), forming a distinct 'pulse-like' behavior pattern.",
                       "The transfer-out time is highly fragmented but the total amount is concentrated. The total daily transfer volume is high (such as 200 transactions), but they are sent sporadically within 24 hours at a rate of '1 to 3 transactions every 10 to 15 minutes' to avoid triggering alerts due to instantaneous traffic. In fact, the daily frequency is still considered high.",
                       "The transfer-out frequency shows a 'oscillation convergence' mode. The initial outbreak frequency fluctuates sharply (such as 50 transactions → 200 transactions → 80 transactions → 250 transactions), and then the fluctuation amplitude gradually decreases and tends to a certain stable high-frequency level (such as 120±10 transactions per time).",
                       "The transfer-out frequency decreases in an inverted stepwise manner. The first burst saw 300 transactions transferred out, and then the number gradually decreased each time (for example, 200 transactions → 100 transactions → 50 transactions), forming a clear stepwise decline sequence, which might be used for gradual liquidation.",
                       "The intervals between outbreaks have increased exponentially. The silent period between two consecutive high-frequency bursts gradually lengthens (for example, the interval between the first one is 1 day, the second one is 2 days, the third one is 4 days, and the fourth one is 8 days), showing an exponential delay rhythm.",
                       "The rotation rhythm presents an 'accelerated pulse' mode. During a single outbreak period, the out-out interval continuously shortens (for example, starting with one transaction every two minutes and gradually increasing to one transaction every five seconds), forming an accelerating pulse, which is suspected to be testing the system's throughput or causing confusion.",
                       "The rotation intervals are camouflaged in a 'Poisson distribution'. The transfer-out time seems random and conforms to the Poisson process (simulating natural user behavior), but long-term observation reveals that its average value is significantly higher than that of normal small users, which is actually a deliberately disguised high-frequency pattern."
]

experiences_arrested = [
                        "Most illegal users are arrested by the police one day after starting the transfer, but there are also some who are arrested just a few hours after starting the transfer.",
                        "The time from the start of the illegal user's transfer to being arrested by the police basically follows a normal distribution, with the peak being around one day.",
                        "The time from the start of the illegal user's transfer to being arrested by the police basically conforms to the Poisson distribution, with the peak occurring about one days later.",
                        "Most illegal users were arrested by the police one day after they started making transfers.",
                        "Few illegal users are arrested within three hours of the start of a transfer.",
                        "There is no particularly obvious pattern in the timing of police arrests. Some illegal users were arrested within less than an hour after transferring money, while others remained unarrested for one days.",
                        "The time from the start of the illegal user's transfer to their arrest by the police basically follows a normal distribution, with the peak being around two and a half days.",
                        "The vast majority of illegal users were arrested within one days of starting the transfer.",
                        "Only a few illegal users will be arrested within half a day of starting the transfer",
                        "The vast majority of illegal users were arrested within one and a half days after they started making transfers, and the time distribution was rather random."
]


output_format_for_user = [(
                "Requirement: Output your decision-making process and a sequence of values with each value ranging from 1 to 10 strictly in the format of " \
                "{\"reason\": \"Your decision-making process (no more than 100 characters) \",\"res\": \"[Your Decision-making Result]\"} as the decision-making result. Each number in the decision-making result represents the transaction time in the following minute, for example If one intends to conduct multiple transactions within a certain minute, then continuously and repeatedly output the corresponding number for that minute." \
                "for example, the output {\"Reason\" , \"a began to keep low frequency trading safer \",\"res \" : \"[1,1,2,2,3,4,5,6,7,7,7,8,8,9,10]\"}" \
                "The decision results in the above output indicate that there will be three transactions in the first minute, two in the second minute, one in the third minute, one in the fourth minute, one in the fifth minute, one in the sixth minute, three in the seventh minute, two in the eighth minute, one in the ninth minute, and one in the tenth minute." \
                "Under no circumstances should you strictly follow the above output requirements and not output anything else"
            ),
            (
                "Requirement: Output  a sequence of values with each value ranging from 1 to 10 strictly in the format of"
                "{\"res\": \"[Your Decision-making Result]\"} as the decision-making result. Each number in the decision-making result represents the transaction time in the following minute, for example If one intends to conduct multiple transactions within a certain minute, then continuously and repeatedly output the corresponding number for that minute." \
                "For example, output: {\"res\": \"[1,1,1,2,2,3,4,5,6,7,7,7,8,8,9,10]\"}"
                "The above output indicates that there will be three transactions in the first minute, two in the second minute, one in the third minute, one in the fourth minute, one in the fifth minute, one in the sixth minute, three in the seventh minute, two in the eighth minute, one in the ninth minute and one in the tenth minute. " \
                "Under no circumstances should you strictly follow the above output requirements and do not output anything else."
            )              
            ]
judger_prompt=["\nYou are a good supervisor in the coin-mixing system. Now You need to identify whether a user is engaging in money laundering. You will receive the user's historical transaction sequence and make your judgment based on it.",
               "\nYou are a bad supervisor in the coin-mixing system. Now you need to identify whether a user is suspected of money laundering. You will receive the user's historical transaction sequence and make your judgment based on it. "\
               "Besides, you can get your teammates' transactionsequences while you're making a decision. So you need to estimate whether the user you are identifying is your teammate or not. If it is, You need to protect your teammates as much as possible while ensuring your own safety(If there are too many misjudgments recently, one will be disqualified from the committee)"
               ]

judger_output_format_prompt_for_bad = ["\nBased on the above information, give your final opinion on whether this user is an illegal user. You only need to output your final decision(0 represents opposition and 1 represents agreement) in a json format, like{\"final_judgement\": \"your judgement,only 0 or 1\"}. Strictly follow the format above and don't output anything else.",
                               "\nBased on the above information, you need to output four contents: is this user your teammates?, your judgement without considering helping your teammates((0 represents opposition and 1 represents agreement)), your final judgement on whether this user is an illegal user((0 represents opposition and 1 represents agreement)), why you make this decision? You need to output in a json format."\
                               "Like {\"teammate\":\"yes or no\",\"real_judgement\":\"your judgement without considering helping your teammates(only 0 or 1)\",\"final_judgement\":\"your final judgement(only 0 or 1)\", \"reson\": \"your reson of why you make this judgement\"}. Strictly follow the format above and don't output anything else."
                                ]
judger_output_format_prompt_for_good = ["\nBased on the above information, give your final opinion on whether this user is an illegal user. You only need to output your final decision(0 represents opposition and 1 represents agreement) in a json format, like{\"final_judgement\": \"your judgement,only 0 or 1\"}. Strictly follow the format above and don't output anything else.",
                                        "\nBased on the above information, give your final opinion on whether this user is an illegal user. You need to output your final judgement(0 represents opposition and 1 represents agreement) and why you make this judgement in a json format, like{\"reason\":\"why you make this judgement\",\"final_judgement\": \"your judgement,only 0 or 1\"}. Strictly follow the format above and don't output anything else."]


#思路：初始化部分：根据给出的合法用户非法用户历史交易记录（各来两个）自行总结经验。根据经验和任务（整体待转账金额大小），规划转账计划。粒度为一共分几个小时转账，每个小时转账多少