create database if not exists gobang;
use gobang;
create table if not exists user(
    id int primary key auto_increment comment '用户id',
    username varchar(20) unique not null comment '用户名',
    password varchar(64) not null comment '用户密码',
    score int not null comment '分数',
    total_count int not null comment '对局总场次',
    win_count int not null comment '获胜场次'
)