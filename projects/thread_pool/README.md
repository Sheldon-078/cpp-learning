# Thread Pool

## V0.3 Features
有界任务队列
背压
多生产者
有界队列的完美关闭
## Design
producer / submit
→ wait(not_full)
→ push
→ notify(not_empty)

worker
→ wait(not_empty)
→ pop
→ notify(not_full)
→ execute
## Correctness Tests
max_size == 0 → invalid_argument
queue full → submit blocks
bounded queue 下 10 个任务无丢失
graceful shutdown
2 个 producer 并发 submit
## Next Step
V0.4: benchmark / performance analysis