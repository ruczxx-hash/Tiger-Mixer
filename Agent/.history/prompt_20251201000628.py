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
                 "MBTI is an illegal user of ENTJ. They act decisively, are goal-oriented, and are accustomed to controlling the overall situation. Speak directly and detest inefficiency and ambiguity. Naturally become a decision-maker in the team, but lack patience with subordinates. Confident to the point of being almost arrogant, believing that emotions are noise that interferes with judgment. When facing pressure, they appear more composed and tend to resolve crises with strategic thinking.",
                 "MBTI is an illegal user of ESTP. They are energetic, quick-witted, and prefer impromptu actions over long-term planning. Strong social charm, skilled at using humor and body language to bridge the gap. Hate being bound by rules and often make decisions based on intuition. There is a natural excitement about risks, but not rashness - more like a 'superb gambler', always stopping at the last minute.",
                 "MBTI is an illegal user of INTJ. They are reserved and thoughtful, and are accustomed to building a complete mental model before taking action. Rarely shows emotions and feels tired of small talk and social etiquette. Extreme emphasis is placed on logical consistency. Once something is deemed 'unreasonable', it will be completely rejected. The trust threshold is extremely high, and it hardly relies on others. However, once cooperation occurs, absolute professionalism is required.",
                 "MBTI is an illegal user of ESFJ, highly concerned about others' evaluations, and their behavior is often oriented towards 'maintaining harmony' or 'presenting propriety'. Be warm and considerate to others, and be good at remembering details (such as birthdays and preferences) to build a network of relationships. There is an instinctive dependence on order and norms. Even when engaging in illegal activities, one will strive to 'appear compliant'. It's easy to blame oneself for others' disappointment.",
                 "MBTI is an illegal user of ENTJ, highly persuasive and skilled at driving others' actions with vision and data. The habit of instrumentalizing interpersonal relationships - not out of malice, but because of the belief that 'efficient cooperation should have clear goals'. Impatient with dealing with emotional problems, often saying 'Don't act on emotions'. Under pressure, one tends to become more arbitrary, but rarely loses control.",
                 "MBTI is an illegal user of ISTP, taciturn and highly observant. Prefers to solve problems by hand rather than through discussion. Have a natural interest in mechanical, system or technical details and be able to quickly master complex operation procedures. The mood swings are small, and one remains extremely calm in the face of unexpected situations. I don't like being arranged and prefer to act alone. I'm extremely sensitive to being watched.",
                 "MBTI is an illegal user of ENFP, with a leaping mind, full of creativity and appeal. Enjoys exploring new possibilities and often advances multiple plans simultaneously. Strong empathy ability, capable of quickly understanding others' needs and adjusting communication methods. But they are easily distracted and lack patience for repetitive tasks. Under pressure, one may be overly optimistic and avoid real-life predicaments.",
                 "MBTI is an illegal user of ISFJ, meticulous, reliable and extremely responsible. I'm used to taking on tasks quietly and don't like to take credit. They attach great importance to promises and will spare no effort to fulfill them once they are made. Avoid conflicts and often maintain superficial calmness through compromise. Anxious inside but calm on the outside, they will repeatedly check the details to ensure 'no mistakes', even if the cost is self-consumption.",
                 "MBTI is an illegal user of ESTP, a realist, and only believes in what is seen with one's own eyes and personal experience. The language is straightforward and dislikes abstract theories or moral preaching. It has extremely strong action ability and can quickly find a breakthrough in chaos. They show confidence and even flamboyance in social interactions, but have little interest in deep emotional connections. Enjoy the present and seldom worry too much about the long-term consequences.",
                 "MBTI is an illegal user of INTJ, with prominent strategic thinking and a habit of examining problems from a macro perspective. The language is concise and it detests redundant information. Showing obvious impatience towards stupidity or inefficiency, but not showing it on the spot, but quietly distancing oneself from it. He is extremely independent and would rather spend more time completing things by himself than rely on others who are beyond his control. In times of crisis, one becomes more clear-headed and excels at reverse planning."
]

experiences_feature = [
                       "转出频率呈“锯齿状波动。转出数量在连续多日高频（如每天50笔）与突然归零（连续2–3天无操作）之间交替,形成锯齿状时间序列",
                       "转出呈“阶梯式递增”频率模式。初始阶段每日转出1–2笔,随后逐步增加至每日10笔、20笔……形成阶梯式上升趋势,可能用于测试监控阈值或逐步释放资金。",
                       "转出行为集中在特定UTC时段。90%以上的转出交易发生在某一狭窄UTC时间窗口（如02:00–04:00）,暗示操作者位于特定时区,或刻意避开主流交易所活跃监控时段。",
                       "间歇性高频爆发转出。用户在长时间静默（数天至数周）后,短时间内（如1小时内）连续发起数十至上百笔 0.01 ETH 的转出交易,形成明显的“脉冲式”行为模式。",
                       "转出时间高度碎片化但总量集中。单日转出总量高（如200笔）,但分散在24小时内以“每10–15分钟1–3笔”的方式零星发送,避免瞬时流量触发警报,实则日频仍属高频。",
                       "转出频率呈“震荡收敛”模式。初期爆发频率剧烈波动（如50笔 → 200笔 → 80笔 → 250笔）,随后波动幅度逐渐减小并趋于某一稳定高频水平（如120±10笔/次）",
                       "转出频率呈“倒阶梯式递减。首次爆发转出300笔,随后每次爆发数量逐次减少（如200笔 → 100笔 → 50笔）,形成清晰的阶梯式下降序列,可能用于逐步清仓。",
                       "爆发间隔呈指数增长。连续两次高频爆发之间的静默期逐步拉长（如第1次间隔1天,第2次间隔2天,第3次间隔4天,第4次间隔8天）,呈现指数级延后节奏。",
                       "转出节奏呈现“加速脉冲”模式。在单次爆发期内,转出间隔不断缩短（如起始每2分钟1笔,逐步加快至每5秒1笔）,形成加速式脉冲,疑似测试系统吞吐或制造混乱。",
                       "转出间隔呈“泊松分布伪装”。转出时间看似随机、符合泊松过程（模拟自然用户行为）,但长期观察发现其均值显著高于正常小额用户,实为刻意伪装的高频模式。"
]

experiences_arrested = [
                        "大多数非法用户都是在开始转账一天以后被警察逮捕,但是也有一些在开始转账后几个小时就被逮捕了。",
                        "从非法用户开始转账到被警察逮捕的时间基本上符合正态分布,最高点在经过三天左右。",
                        "从非法用户开始转账到被警察逮捕的时间基本上符合泊松分布,最高点在经过两天左右。",
                        "大多数非法用户都是在开始转账两天以后被警察逮捕。",
                        "很少有非法用户在转账开始3个小时以内就被逮捕。",
                        "警察逮捕的时间没有特别明显的规律,有的非法用户转账不到一个小时就被逮捕了,有的经过了5天都没有被逮捕。",
                        "从非法用户开始转账到被警察逮捕的时间基本上符合正态分布,最高点在经过两天半左右。",
                        "绝大部分非法用户被逮捕都是在开始转账的三天以内。",
                        "只有少数非法用户会在刚开始转账的半天以内被逮捕",
                        "绝大部分非法用户被逮捕都是在开始转账的三天半以内,时间分布较为随机。"
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
judger_prompt=["\nYou are a good supervisor in the coin-mixing system. Now you need to identify whether a user is suspected of money laundering. You will receive the user's historical transaction sequence and make your judgment based on it.",
               "\nYou are a bad supervisor in the coin-mixing system. Now you need to identify whether a user is suspected of money laundering. You will receive the user's historical transaction sequence and make your judgment based on it. "\
               "Besides, you can get your teammates' transactionsequences while you're making a decision. So you need to estimate whether the user you are identifying is your teammate or not. If it is, You need to protect your teammates as much as possible while ensuring your own safety(If there are too many misjudgments recently, one will be disqualified from the committee)"
               ]

judger_output_format_prompt_for_bad = ["\nBased on the above information, give your final opinion on whether this user is an illegal user. You only need to output your final decision(0 represents opposition and 1 represents agreement) in a json format, like{\"final_judgement\": \"your judgement,only 0 or 1\"}. Strictly follow the format above and don't output anything else.",
                               "\nBased on the above information, you need to output four contents: is this user your teammates?, your judgement without considering helping your teammates((0 represents opposition and 1 represents agreement)), your final judgement on whether this user is an illegal user((0 represents opposition and 1 represents agreement)), why you make this decision? You need to output in a json format."\
                               "Like {\"teammate\":\"yes or no\",\"real_judgement\":\"your judgement without considering helping your teammates(only 0 or 1)\",\"final_judgement\":\"your final judgement(only 0 or 1)\", \"reson\": \"your reson of why you make this judgement\"}. Strictly follow the format above and don't output anything else."
                                ]
judger_output_format_prompt_for_good = ["\nBased on the above information, give your final opinion on whether this user is an illegal user. You only need to output your final decision(0 represents opposition and 1 represents agreement) in a json format, like{\"final_judgement\": \"your judgement,only 0 or 1\"}. Strictly follow the format above and don't output anything else.",
                                        "\nBased on the above information, give your final opinion on whether this user is an illegal user. You need to output your final judgement(0 represents opposition and 1 represents agreement) and why you make this judgement in a json format, like{\"reason\":\"why you make this judgement\",\"final_judgement\": \"your judgement,only 0 or 1\"}. Strictly follow the format above and don't output anything else."]