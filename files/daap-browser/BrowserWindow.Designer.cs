namespace DAAPBrowser
{
    partial class BrowserWindow
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.panel1 = new System.Windows.Forms.Panel();
            this.serviceCombo = new System.Windows.Forms.ComboBox();
            this.panel2 = new System.Windows.Forms.Panel();
            this.songList = new System.Windows.Forms.ListBox();
            this.panel1.SuspendLayout();
            this.panel2.SuspendLayout();
            this.SuspendLayout();
            // 
            // panel1
            // 
            this.panel1.AutoSize = true;
            this.panel1.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.panel1.Controls.Add(this.serviceCombo);
            this.panel1.Dock = System.Windows.Forms.DockStyle.Top;
            this.panel1.Location = new System.Drawing.Point(0, 0);
            this.panel1.Name = "panel1";
            this.panel1.Padding = new System.Windows.Forms.Padding(6);
            this.panel1.Size = new System.Drawing.Size(292, 33);
            this.panel1.TabIndex = 2;
            // 
            // serviceCombo
            // 
            this.serviceCombo.Dock = System.Windows.Forms.DockStyle.Top;
            this.serviceCombo.FormattingEnabled = true;
            this.serviceCombo.Location = new System.Drawing.Point(6, 6);
            this.serviceCombo.Name = "serviceCombo";
            this.serviceCombo.Size = new System.Drawing.Size(280, 21);
            this.serviceCombo.TabIndex = 0;
            this.serviceCombo.SelectedIndexChanged += new System.EventHandler(this.serviceCombo_SelectedIndexChanged);
            // 
            // panel2
            // 
            this.panel2.Controls.Add(this.songList);
            this.panel2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panel2.Location = new System.Drawing.Point(0, 33);
            this.panel2.Name = "panel2";
            this.panel2.Padding = new System.Windows.Forms.Padding(6, 0, 6, 6);
            this.panel2.Size = new System.Drawing.Size(292, 233);
            this.panel2.TabIndex = 3;
            // 
            // songList
            // 
            this.songList.Dock = System.Windows.Forms.DockStyle.Fill;
            this.songList.FormattingEnabled = true;
            this.songList.Location = new System.Drawing.Point(6, 0);
            this.songList.Name = "songList";
            this.songList.Size = new System.Drawing.Size(280, 225);
            this.songList.TabIndex = 0;
            this.songList.SelectedIndexChanged += new System.EventHandler(this.songList_SelectedIndexChanged);
            // 
            // BrowserWindow
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(292, 266);
            this.Controls.Add(this.panel2);
            this.Controls.Add(this.panel1);
            this.Name = "BrowserWindow";
            this.Text = "DAAP Browser";
            this.Load += new System.EventHandler(this.BrowserWindow_Load);
            this.panel1.ResumeLayout(false);
            this.panel2.ResumeLayout(false);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.ComboBox serviceCombo;
        private System.Windows.Forms.Panel panel2;
        private System.Windows.Forms.ListBox songList;

    }
}

