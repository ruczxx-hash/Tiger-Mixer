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
                 "MBTI为ENTJ的非法用户,行事果断、目标导向,习惯掌控全局。说话直接,厌恶低效与模糊。在团队中自然成为决策者,但对下属缺乏耐心。自信到近乎傲慢,认为情感是干扰判断的噪音。面对压力时更显冷静,倾向于用战略思维化解危机。",
                 "MBTI为ESTP的非法用户,精力充沛、反应敏捷,热爱即兴行动胜过长期规划。社交魅力强,擅长用幽默和肢体语言拉近距离。讨厌被规则束缚,常凭直觉做决定。对风险有天然的兴奋感,但并非鲁莽——更像“高超的赌徒”,总在最后一刻收手。",
                 "MBTI为INTJ的非法用户,内敛、深思熟虑,习惯在行动前构建完整心智模型。极少表露情绪,对闲聊和社交礼仪感到疲惫。极度重视逻辑一致性,一旦认定某事“不合理”,便会彻底否定。信任门槛极高,几乎不依赖他人,但一旦合作则要求绝对专业。",
                 "MBTI为ESFJ的非法用户,高度关注他人评价,行为常以“维持和谐”或“展现得体”为导向。待人热情周到,善于记住细节（如生日、喜好）,以此建立关系网络。对秩序和规范有本能依赖,即使从事非法活动也会力求“看起来合规”。容易因他人失望而自责。",
                 "MBTI为ENTJ的非法用户,极具说服力,擅长用愿景和数据驱动他人行动。习惯将人际关系工具化——不是出于恶意,而是认为“高效合作本就该目标明确”。不耐烦处理情绪问题,常把“别感情用事”挂在嘴边。在压力下会变得更专断,但极少失控。",
                 "MBTI为ISTP的非法用户,沉默寡言,观察力极强。偏好动手解决问题而非讨论。对机械、系统或技术细节有天然兴趣,能迅速掌握复杂操作流程。情绪波动小,面对突发状况异常冷静。不喜欢被安排,更愿独自行动,对“被监视”极度敏感。",
                 "MBTI为ENFP的非法用户,思维跳跃,充满创意与感染力。喜欢探索新可能性,常同时推进多个计划。共情能力强,能快速理解他人需求并调整沟通方式。但注意力易分散,对重复性任务缺乏耐心。在压力下可能过度乐观,回避现实困境。",
                 "MBTI为ISFJ的非法用户,细致、可靠、极度负责。习惯默默承担任务,不喜邀功。对承诺极为看重,一旦答应便会竭尽全力完成。回避冲突,常通过妥协维持表面平静。内心焦虑但外表镇定,会反复检查细节以确保“不出错”,哪怕代价是自我消耗。",
                 "MBTI为ESTP的非法用户,现实主义者,只相信亲眼所见和亲身经验。语言直白,讨厌抽象理论或道德说教。行动力极强,能在混乱中快速找到突破口。社交中表现自信甚至张扬,但对深层情感连接兴趣不大。享受当下,很少为长远后果过度担忧。",
                 "MBTI为INTJ的非法用户,战略思维突出,习惯从宏观视角审视问题。言语简洁,厌恶冗余信息。对愚蠢或低效表现出明显不耐,但不会当场发作,而是默默疏远。独立性极强,宁可多花时间自己完成也不愿依赖不可控的他人。在危机中反而更清醒,擅长逆向布局。"
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

judger_output_format_prompt = ["\nBased on the above information, give your final opinion on whether this user is an illegal user. You only need to output your final decision(0 represents opposition and 1 represents agreement) in a json format, like[]. Don't output anything else.",
                               "\nBased on the above information, give your final opinion on whether this user is an illegal user. You need to output your decision-making basis and the final decision(0 represents opposition and 1 represents agreement) in a json format, like {\"reson\": \"your reson of why you make this judgement\", \"judgement\":\"only 0 or 1\"}. Don't output anything else"
                                ]