using System.Collections.ObjectModel;
using NativeTimelapseGenerator.Console.Delegates;
using NativeTimelapseGenerator.Console.Structs;
using Terminal.Gui;

namespace NativeTimelapseGenerator.Console;

public class ControlWindow : Window
{
    public ObservableCollection<string> ServerLogs { get; } = [];

    private readonly StartCallbackDelegate startCallback;
    private readonly CallbackDelegate shutdownCallback;
    private readonly WorkersCountCallbackDelegate addWorkerCallback;
    private readonly WorkersCountCallbackDelegate removeWorkerCallback;
    private bool disposed;

    public ControlWindow(StartCallbackDelegate startCallback, CallbackDelegate shutdownCallback,
        WorkersCountCallbackDelegate addWorkerCallback, WorkersCountCallbackDelegate removeWorkerCallback)
    {
        this.startCallback = startCallback;
        this.shutdownCallback = shutdownCallback;
        this.addWorkerCallback = addWorkerCallback;
        this.removeWorkerCallback = removeWorkerCallback;
        
        InitialiseWindow();
    }

    private void InitialiseWindow()
    {
        X = 0;
        Y = 0;
        Width = Dim.Fill();
        Height = Dim.Fill();
        BorderStyle = LineStyle.Rounded;
        Title = "NativeTimelapseGenerator Control Panel (Ctrl+Q to quit)";

        var shutdownBtn = new Button()
        {
            Text = "Shutdown",
            Y = 1,
            X = Pos.Center() + 6,
        };
        shutdownBtn.MouseClick += (_, _) =>
        {
            MessageBox.Query("Shutdown", "Halting backups generation", "Okay");
            Application.Shutdown();
            shutdownCallback.Invoke();
        };

        var startBtn = new Button()
        {
            Text = "Start",
            Y = 1,
            X = Pos.Center() - 12
        };
        startBtn.MouseClick += (_, _) =>
        {
            ShowStartDialog();
        };

        var workerPanel = new View()
        {
            Y = 4,
            X = 1,
            Height = 4,
            Width = Dim.Percent(50)! - 2,
            BorderStyle = LineStyle.Rounded,
            Title = "Workers"
        };

        var downloadLabel = new Label()
        {
            Text = "Download Workers:",
            Y = 0,
            X = 1,
        };
        var downloadWorkerLabel = new Label()
        {
            Text = "0",
            Y = 0,
            X = Pos.Right(downloadLabel),
        };
        var renderLabel = new Label()
        {
            Text = "Render workers:",
            Y = 1,
            X = 1,
        };
        var renderWorkerLabel = new Label()
        {
            Text = "0",
            Y = 1,
            X = Pos.Right(renderLabel),
        };
        var saveLabel = new Label()
        {
            Text = "Save workers: ",
            Y = 2,
            X = 1,
        };
        var saveWorkerLabel = new Label()
        {
            Text = "0",
            Y = 2,
            X = Pos.Right(saveLabel),
        };

        var addButton = new Button()
        {
            Text = "Add worker",
            Y = 3,
            X = 0,
        };
        addButton.MouseClick += (_, _) =>
        {
            var selected = MessageBox.Query("Add worker", "Select step to add single worker to", "Download", "Render", "Save", "Cancel");
            if (selected != 3)
            {
                addWorkerCallback.Invoke(selected, 1);
            }
        };

        var removeButton = new Button()
        {
            Text = "Remove worker",
            Y = 3,
            X = Pos.Percent(50)
        };
        removeButton.MouseClick += (_, _) =>
        {
            var selected = MessageBox.Query("Remove worker", "Select step to remove single worker from", "Download", "Render", "Save ", "Cancel");
            if (selected != 3)
            {
                removeWorkerCallback.Invoke(selected, 1);
            }
        };

        workerPanel.Add(downloadLabel, downloadWorkerLabel, renderLabel, renderWorkerLabel, saveLabel, saveWorkerLabel, addButton, removeButton);

        var backupPanel = new View()
        {
            Y = 4,
            X = Pos.Percent(50) + 2,
            Height = 4,
            Width = Dim.Percent(50)! - 2,
            BorderStyle = LineStyle.Rounded,
            Title = "Backups"
        };

        var backupsTotal = new Label()
        {
            Text = "Total frames:",
            Y = 0,
            X = 1,
        };
        var backupsTotalLabel = new Label()
        {
            Text = "0",
            Y = 0,
            X = Pos.Right(backupsTotal),
        };
        var backupsPs = new Label()
        {
            Text = "Backups per second:",
            Y = 1,
            X = 1,
        };
        var backupsPsLabel = new Label()
        {
            Text = "0",
            Y = 1,
            X = Pos.Right(backupsPs),
        };
        var currentDate = new Label()
        {
            Text = "Current canvas date:",
            Y = 2,
            X = 1,
        };
        var currentDateLabel = new Label()
        {
            Text = "null",
            Y = 2,
            X = Pos.Right(currentDate),
        };

        backupPanel.Add(backupsTotal, backupsTotalLabel, backupsPs, backupsPsLabel, currentDate, currentDateLabel);

        var serverLogListView = new ListView()
        {
            Width = Dim.Fill()
        };
        serverLogListView.SetSource(ServerLogs);
        var serverLogPanel = new FrameView()
        {
            Y = Pos.AnchorEnd() - 12,
            Height = 10,
            BorderStyle = LineStyle.Rounded,
            Title = "Server logs",
        };
        serverLogPanel.Add(serverLogListView);

        var clearBtn = new Button()
        {
            Text = "Clear",
            Y = Pos.AnchorEnd() - 12,
            X = Pos.AnchorEnd() - 12,
        };
        clearBtn.MouseClick += (_, _) =>
        {
            ServerLogs.Clear();
        };

        Add(shutdownBtn, startBtn, workerPanel, backupPanel, serverLogPanel, clearBtn);
    }
    
    private void OpenWizard(Wizard wizard)
    {
        Application.Top?.Add(wizard);
    }

    private void CloseWizard(Wizard wizard)
    {
        Application.Top?.Remove(wizard);
    }
    
    private void ShowStartDialog()
    {
        var wizard = new Wizard
        {
            Title = "Start backup generation",
            Modal = true,
            Width = 32,
            Height = 12,
            BorderStyle = LineStyle.Rounded
        };

        var repoUrlLabel = new Label()
        {
            Text = "Repo URL: ",
            Y= 0,
        };
        var repoUrlField = new TextField()
        {
            Text = "https://github.com/rplacetk/canvas1",
            Y= 0,
            X = Pos.Right(repoUrlLabel),
        };
        var downloadBaseUrlLabel = new Label()
        {
            Text = "Download base URL: ",
            Y= 1,
        };
        var downloadBaseUrlField = new TextField()
        {
            Text = "https://raw.githubusercontent.com/rplacetk/canvas1",
            Y= 1,
            X = Pos.Right(downloadBaseUrlLabel),
        };
        var commitHashesFileNameLabel = new Label()
        {
            Text = "Commit hashes file name: ",
            Y = 2,
        };
        var commitHashesFileNameField = new TextField()
        {
            Text = "commit_hashes.txt",
            Y= 2,
            X = Pos.Right(commitHashesFileNameLabel),
        };
        var gameServerBaseUrlLabel = new Label()
        {
            Text = "Game server base URL: ",
            Y = 3,
        };
        var gameServerBaseUrlField = new TextField()
        {
            Text = "https://server.rplace.live",
            Y= 3,
            X = Pos.Right(gameServerBaseUrlLabel),
        };
        var maxTopPlacersLabel = new Label()
        {
            Text = "Max top placers: ",
            Y = 4,
        };
        var maxTopPlacersInput = new NumericUpDown()
        {
            Value = 10,
            Y= 4,
            X = Pos.Right(maxTopPlacersLabel),
        };
        wizard.Add(
            repoUrlLabel,
            repoUrlField,
            downloadBaseUrlLabel,
            downloadBaseUrlField,
            commitHashesFileNameLabel,
            commitHashesFileNameField,
            gameServerBaseUrlLabel,
            gameServerBaseUrlField,
            maxTopPlacersLabel,
            maxTopPlacersInput
        );
        
        wizard.BackButton.MouseClick += (_, _) =>
        {
            CloseWizard(wizard);
        };
        wizard.Finished += (_, _) =>
        {
            var config = new Config()
            {
                RepoUrl = repoUrlField.Text,
                DownloadBaseUrl = downloadBaseUrlField.Text,
                CommitHashesFileName = commitHashesFileNameField.Text,
                GameServerBaseUrl = gameServerBaseUrlField.Text,
                MaxTopPlacers = maxTopPlacersInput.Value
            };
            
            if (MessageBox.Query("Start", "Starting backups generation", "Okay") == 0)
            {
                startCallback.Invoke(config);
                CloseWizard(wizard);
            }
        };

        OpenWizard(wizard);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposed)
        {
            return;
        }

        shutdownCallback.Invoke();
        disposed = true;
    }
}
