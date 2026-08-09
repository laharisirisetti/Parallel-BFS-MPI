# Parallel BFS algorithm design thinking

## Main decisions
- Splitting vertices among all processors. Also thri adjacency list. With this choice, Now Every processor is responsible for only the vertices that assigned to it. While doing BFS, if it encounters other vertex which is not belonged to it, then it should sent to other processor who own's it, so here every processor will know who owns that partical vertex.
- **Need Design** - How to split vertices emong processors efficiently
- Now, after root taking input graph, it will split veritces and broadcast that info to everyone. Also adjaceny matrix;
- Now after receiving inputs, finally every node starts it's local BFS.
- So here in the step of BFS we need to check whther there are any node in current level - so here we should not only check local current levl nodes, because if we do that and end the BFS of that processor, but other processor still doing.. it may enconter child which are related to this process.
so to solve this one thought which came is, instead of checking current level, each processor may validate visited node size with their total nodes assigned. But this fails - since each level should be coordinated amon all processors, if one processor nodes done early level, and other processors running next levels, they will wait at each level to make BFS work pr=erfectly, but since some processor end their process, they won't complete those next levels, and these other processor will just wait for ever and code hangs. So second thought what I am thinking is, at each level every process sends their current level size, using allReduce SUM operation (or max) operation - we will get global current level node size, based on that they will decide whether BFS finished  not.
- now after entering BFS-> nodes will process their local current level nodes -> every process will have level variable to know levle number. So they will assign distance accordingly. Here another thght what I got is in normal BFS, we don't need this variable, because we can just assign child dist as +1 of parent, bet here we can't do that because, at each level processors may receive child node and the parent node doesnot belong to it, then the other proces who owns need send the dist of parent - which increases communication. so instead of that each process can just mainain current level number.
- so now coming to processing current nodes, dist of node will be updated by current level. and traversal happen thorugh that node children, for each childer first processor checks if it belongs to it or not. 1. If it belong to it, then it will check whther it is already visited or not, if not visited, then pushes into next level queue.
2.If it not belongs to then, it will prepare a map of other processor and theri children enocunterd in current level. After finising all current nodes, then it will send the child node that are not belongs to it to the other processors - so here actually we need to think through, like may only some processes need to get this list, and each process don't know whether it is going to receive from any source or not. without if just waits to recieve from all nodes, then process may hang. Here some options I am thing to solve- 1. here each process sends their other processors child list to everyone, then we can safely put receive message from processorSize - 1 times, BUt this is worst because, each level there are sz - 1 send and receives. 2. here what we can do is, instead of sending whole list to everyone, processor will send list a single list to every processor, that list contains process 0 owning child size, proce1 holding child size, --- till processn-1. So now every processor receives this list, and based on that they will wait to receive from the processors list, in which this process number size is greater than 0. we can match messages reach correct postion by tags. - then here messages are still n-1 send and n-1 recieve for the initial list, but buffer is low compared to previous approaches. and then addition messages based on size to each processor. So I am thinking to use this approach. 3. I am thing if ther is any possibility like each processor will send size and list to processes size > 0, and is there a way like in particalr interval, we can process all messages recived dynamically without knowing whetehr we will recive messages or not? if yes then less communication may happen.
- So now everyone fter recieving child nodes it owns from other processsor, they will add to next quue, if they are not alredy visited. here we will put barrie, so that every node completes the level successfully. then increasing lvel variable locally, and proceeding to second level again.. as said already next level depends on global size.

- after BFS finishes, each node sends their results to root, and root will have all vertexe shortes path from source.


1. Read graph (Root)

        ↓

2. Partition vertices among processes

        ↓

3. Distribute graph partitions

        ↓

4. Initialize source vertex
   (Owner process starts BFS)

        ↓

5. While global frontier is not empty

      ├── Process local frontier
      │
      ├── Build outgoing messages
      │
      ├── Exchange messages
      │
      ├── Process received vertices
      │
      ├── Form next frontier
      │
      └── Check global termination

        ↓

6. Gather local distance arrays

        ↓

7. Root assembles final result