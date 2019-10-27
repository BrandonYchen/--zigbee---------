from flask_cors import CORS
from flask import Flask, request, jsonify,make_response
import socket
import struct
import json

SendBuf = [0x3A,0x00,0xFF,0x01,0xC4,0x23]
NodeData = []
global timer

app = Flask(__name__)
# r'/*' 是通配符，让本服务器所有的 URL 都允许跨域请求
CORS(app, resources=r'/*')

@app.route('/access',methods=["GET","POST"])
def forResponse():
    try:
        # print(request.args)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(('192.168.0.101', 33333))
        req = struct.pack("%dB"%(len(SendBuf)),*SendBuf)
        sock.send(req)
        print("连接发送成功")

        RxBuf = sock.recv(1024)
        print("接收成功！接收到的数据：")
        print(RxBuf)
        start3A = ord(RxBuf[0:1])
        if start3A == 58:    #判断第一个是否为0x3A
            # print("第1个为0x3A")

            #解析数据
            if RxBuf[3] == 0x01:
                # print("第4个为0x01")

                #光照强度
                Light_High = RxBuf[12]
                Light_Low = RxBuf[13]

                l_high = bin(Light_High).replace('0b', '')
                print(l_high)

                l_low = bin(Light_Low).replace('0b','')
                while len(l_low) != 8:
                    l_low = '0' + l_low
                print(l_low)

                light_er = l_high + '' + l_low
                print(light_er)
                light_shi = int(light_er,2)

                obj = {"wendu":RxBuf[4],"shidu":RxBuf[5],"yuliang":RxBuf[8],"guangzhao":light_shi}
                jsonobj = json.dumps(obj,ensure_ascii=False,indent=4)
                # jsonobj = make_response(jsonobj)
                print(jsonobj)

                return jsonobj
        else:
            return "查询失败，请您稍后再查询"

        sock.close()  # 关闭socket连接

    except Exception as e:
        print(e)

if __name__ == '__main__':
    app.run(port=5000,debug=True)