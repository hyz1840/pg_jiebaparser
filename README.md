# 另一款PostgreSQL中文分词插件

------
## 产生此项目的原因
能找到的中文pg分词插件主要存在如下问题：
> * 内存使用低效，
    由于pg数据库服务是process模式，每个用户连接会创建一个process，分词插件需要加载大量词库和内存数据结构到进程内存空间中， 无法适应大量的用户连接。
> * 加载速度慢，由于中文特性，词库或者模型较大，创建连接(或者第一次进行插件调用)时速度慢。
## 基本逻辑

分词引擎采用较流行的jieba分词C++版本， 在此基础上重构了内部数据结构，全部的词库生成的数据结构全部置于共享内存。

## 技术细节

> * 为适应共享内存方式，重构分词引擎内部数据结构，去除所有指针和引用，使用随机访问索引值方式。
> * 首次加载插件加载词库，构建数据结构，写入共享内存映射文件。
> * 后期使用，其他process使用，均直接映射共享内存，不需加载词库，不需重建数据结构。
> * 用户可以更新自定义词库，插件提供接口重新构建共享内存数据。
> * 共享内存数据具备版本定义，插件版本更新或者词库变更可在数据库连接创建或者第一次使用分词功能时自动重建。
> * 充分考虑了多进程加载(或重建)共享内存数据的同步。

## 达成效果
> * 每个process加载插件实例内存增加小于2M (原始jieba引擎占用150M/windows, 190M/linux)。
> * 非首次加载速度小于100ms（I7 8700 cpu)。
> * 分词速度比原始jieba版本有微量下降(<10%, 由于内部数据结构增加一层间接查找关联)。

## 编译
注意需要修改CMakeList.txt中boost库相关路径

```shell
mkdir build
cd build

cmake ..

make
make install 
```

## 使用
```sql
CREATE EXTENSION jiebaparser;

--分词测试
--MixSegment Mode
select ts_parse('jiebaparser','小明硕士毕业于中国科学院计算所，后在日本京都大学深造.')
--QuerySegment mode
select ts_parse('jiebaparserqry','小明硕士毕业于中国科学院计算所，后在日本京都大学深造.')

--全文检索向量生成测试
--MixSegment mode
select * from to_tsvector('jbtscfg','是拖拉机学院手扶拖拉机专业的。不用多久，我就会升职加薪，当上CEO，走上人生巅峰。')
--QuerySegment mode
select * from to_tsvector('jbqrytscfg','是拖拉机学院手扶拖拉机专业的。不用多久，我就会升职加薪，当上CEO，走上人生巅峰。')

--重建共享内存数据结构
select jiebaparser_reset()

```

## 其他项目
[pg_jieba][1]

    


[zhparser][2]


  [1]: https://github.com/jaiminpan/pg_jieba
  [2]: https://github.com/amutu/zhparser