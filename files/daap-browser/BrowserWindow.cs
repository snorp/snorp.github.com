using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using DAAP;
using WMPLib;

namespace DAAPBrowser
{
    public partial class BrowserWindow : Form
    {
        private ServiceLocator locator;

        public BrowserWindow()
        {
            InitializeComponent();
        }

        private void BrowserWindow_Load(object sender, EventArgs e)
        {
            locator = new ServiceLocator ();
            locator.Found += OnServiceFound;
            locator.Removed += OnServiceRemoved;
            locator.Start ();
        }

        private void OnServiceRemoved (object o, ServiceArgs args) {
            if (InvokeRequired)
            {
                Invoke(new ServiceHandler(OnServiceFound), o, args);
            }
            else
            {
                Console.WriteLine("Found service: " + args.Service);
                serviceCombo.Items.Remove(args.Service);
            }
        }

        private void OnServiceFound (object o, ServiceArgs args) {
            if (InvokeRequired)
            {
                Invoke(new ServiceHandler(OnServiceFound), o, args);
            } else {
                Console.WriteLine ("Found service: " + args.Service);
                serviceCombo.Items.Add(args.Service);
            }
        }

        private void serviceCombo_SelectedIndexChanged(object sender, EventArgs e)
        {
            LoadService((Service) serviceCombo.Items[serviceCombo.SelectedIndex]);
        }

        private void LoadService(Service service)
        {
            songList.Items.Clear();

            Client client = new Client(service);
            client.Login();

            foreach (Song song in client.Databases[0].Songs)
            {
                songList.Items.Add (String.Format ("{0} - {1} - {2}",
                    song.Artist, song.Album, song.Title));
            }
        }

        private void songList_SelectedIndexChanged(object sender, EventArgs e)
        {

        }
    }
}