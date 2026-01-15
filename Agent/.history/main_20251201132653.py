import Log
import User
from config import total_moneys
import prompt
import re
import threading
import Judger
from flask import Flask, request, jsonify

app = Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = 16 * 1024 * 1024
lock = threading.Lock()

badusers = []
judgers = []
user_count = 0


@app.route('/getAccountTransactions', methods=['POST'])
def getAccountTransactions():
    global user_count, badusers
    data = request.get_json(silent=True)
    message = ""
    if not data or 'id' not in data:
        print("error in data")
        return jsonify(error="not enough information"), 400
    # 正确检查 data 是否存在且包含 'addition_info'
    if 'addition_info' not in data:
        addition_info = ""
    else:
        addition_info = data['addition_info']
    try:
        exist = False
        for i in badusers:
            if i.user_id == data['id']:
                message = i.call_llm(addition_info)  # 局部变量,无需 global
                exist = True
                break
        if not exist:
            with lock:
                new_user = User.BadUser('''prompt.experiences_feature[user_count % 10]'''" ", prompt.experiences_arrested[user_count % 10],
                                   prompt.personalities[user_count % 10], data['id'], total_moneys[user_count % 10],
                                   user_count)
                message = new_user.call_llm(addition_info)
                badusers.append(new_user)
                user_count += 1

    except Exception as e:
        import traceback
        traceback.print_exc()
        Log.logging.error(f"Error in getAccountTransactions: {e}")
        return jsonify(error=str(e)), 400

    return jsonify(message=message), 200


@app.route('/failedTransactions', methods=['POST'])
def failedTransactions():
    data = request.get_json(silent=True)
    print(data)
    if not data or 'id' not in data or 'group' not in data or 'rank' not in data:
        return jsonify(error="Not enough information"), 400
    try:
        group = int(data['group']) - 1
        rank = int(data['rank'])
        count = 0
        for j in badusers:
            if j.user_id == data['id']:
                for i in j.transaction_list:
                    if i.time / 10 == group:
                        count = count + i.send
                    if count >= rank:
                        i.update_failed()
                        break
                j.money_transferred = j.money_transferred - 0.01
    except Exception:
        return jsonify(error=Exception), 400
    return "success", 200


@app.route('/register',methods=['POST'])
def register():
    data = request.get_json(silent=True)
    if not data or 'id' not in data:
        return jsonify(error="not enough parameters"), 400
    new_user = User.BadUser('''prompt.experiences_feature[user_count % 10]'''" ", prompt.experiences_arrested[user_count % 10],
                                prompt.personalities[user_count % 10], data['id'], total_moneys[user_count % 10],
                                user_count)
            message = new_user.call_llm(addition_info)
    with lock:
            
            badusers.append(new_user)
            user_count += 1

@app.route('/arrested', methods=['POST'])
def arrested():
    data = request.get_json(silent=True)
    if not data or 'id' not in data:
        return jsonify(error="not enough parameters"), 400
    try:
        for i in badusers:
            if i.user_id == data['id']:
                i.arrested_list_append()
                break

        for i in badusers:
            i.handle_new_arrest()

    except Exception:
        return jsonify(error="something wrong about handle the outside arrest"), 400
    return jsonify(message="complete"), 200


@app.route('/intercepted', methods=['POST'])
def intercepted():
    global illegal_list
    data = request.get_json(silent=True)
    if not data or 'id' not in data:
        return jsonify(error="not enough parameters"), 400
    try:
        for i in badusers:
            if i.user_id != data['id']:
                i.handle_new_interception()

    except Exception:
        return jsonify(error="something wrong about handle the internal interception"), 400
    return jsonify(message="complete"), 200


@app.route('/judge', methods=['POST'])
def judge_illegal():
    global judgers
    data = request.get_json(silent=True)
    print(data)
    if not data or 'record' not in data or 'id' not in data:
        print("error here")
        return jsonify(error="not enough parameters"), 400
    else:
        # 解析 pairs_summary 格式: "(0,1)(1,1)(2,1)..."
        record_str = data['record']
        # 使用正则表达式提取所有 (秒索引,数量) 对
        pattern = r'\((\d+),(\d+)\)'
        matches = re.findall(pattern, record_str)
        # 转换为元组列表
        transaction_sequence = [(int(second), int(count)) for second, count in matches]
        transaction_list = []
        for i in transaction_sequence:
            record = "In" + str(i[0]) + "minute, do " + str(i[1]) + "times transactions"
            transaction_list.append(record)

        bad_user_sequence = []
        for i in badusers:
            bad_user_sequence.append({"id":i.user_id,"sequence":i.transaction_list})
            
        user_id = data['id']
        for i in judgers:
            if i.id == user_id:
                if i.judge(transaction_list,bad_user_sequence):
                    return jsonify(message="1"), 200
                else:
                    return jsonify(message="0"), 200
        
        is_bad = False
        for i in badusers:
            if i.user_id == user_id:
                is_bad = True
                break
            
        new_judger = Judger.Judger(user_id,is_bad)
        judgers.append(new_judger)
        if new_judger.judge(transaction_list,bad_user_sequence):
            return jsonify(message="1"), 200
        else:
            return jsonify(message="0"), 200


if __name__ == '__main__':
    app.run(host="0.0.0.0", port=8001)
