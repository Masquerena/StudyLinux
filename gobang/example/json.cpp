#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <jsoncpp/json/json.h>

std::string serialize()//序列化
{
    //1. 生成一个Json::Value对象，将要序列化的数据放入其中
    Json::Value root;
    root["姓名"] = "张三";
    root["年龄"] = 18;
    root["成绩"].append(98);
    root["成绩"].append(80);
    root["成绩"].append(90);
    //2. 实例化一个StreamWriterBuilder工厂类对象
    Json::StreamWriterBuilder swb;
    //3. 通过StreamWriterBuilder工厂类对象生产一个StreamWriter对象
    Json::StreamWriter *sw = swb.newStreamWriter();
    //4. 通过StreamWriter对象对Json::Value中存储的数据进行序列化
    std::stringstream ss;
    int ret = sw->write(root, &ss);
    if(ret != 0)
    {
        std::cout << "json serialize failed" << std::endl;
        return nullptr;
    }

    // std::cout << ss.str() << std::endl;
    delete sw;//销毁对象

    return ss.str();
}

void unserialize(const std::string str)
{
    //1. 实例化一个CharReaderBuilder工厂类对象
    Json::CharReaderBuilder crb;
    //2. 通过CharReaderBuilder对象生产一个CharReader对象
    Json::CharReader *cr = crb.newCharReader();
    //3. 实例化一个Json::Value对象存储反序列化后的数据
    Json::Value root;
    std::string errs;
    //4. 使用CharReader对象中parse接口反序列化
    bool ret = cr->parse(str.c_str(), str.c_str() + str.size(), &root, &errs);
    if(ret == false)
    {
        std::cout << "json unserialize failed" << std::endl;
        return;
    }
    //5. 逐个元素的访问反序列化后的数据
    std::cout << "姓名: " << root["姓名"].asString() << std::endl;
    std::cout << "年龄" << root["年龄"].asInt() << std::endl;
    int sz = root["成绩"].size();
    for(int i = 0; i < sz; ++i)
        std::cout << "成绩" << root["成绩"][i].asFloat() << std::endl;

    delete cr;
}

int main()
{
    std::string str = serialize();
    unserialize(str);

    return 0;
}