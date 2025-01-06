namespace NativeTimelapseGenerator.Console;

public class WorkerInfo
{
    public string Name { get; set; }
    public int Count { get; set; }

    public WorkerInfo(string name, int count)
    {
        Name = name;
        Count = count;
    }
}