const {app, ipcMain, protocol, BrowserWindow, dialog} = require('electron');

const fs = require('fs');
const path = require('path');
const url = require('url');
const log = require('electron-log');

let mainWindow;

log.transports.file.level = 'info' 
log.transports.console.level = 'info'

const squirrelEvent = process.argv[1];
log.info('Squirrel event: ' + squirrelEvent);

if(require('electron-squirrel-startup'))
{
    return;
}

function interceptFileProtocol() {
    // Intercept the file protocol so that references to folders return its index.html file
    const fileProtocol = 'file';
    const cwd = process.cwd();
    protocol.interceptFileProtocol(fileProtocol, (request, callback) => {
        const fileUrl = new url.URL(request.url);
        const hostname = decodeURI(fileUrl.hostname);
        const filePath = decodeURI(fileUrl.pathname);
        let resolvedPath = path.normalize(filePath);
        if (resolvedPath[0] === '\\') {
            // Remove URL host to pathname separator
            resolvedPath = resolvedPath.substr(1);
        }
        if (hostname) {
            resolvedPath = path.join(hostname, resolvedPath);
            if (process.platform === 'win32') {  // File is on a share
                resolvedPath = `\\\\${resolvedPath}`;
            }
        }
        resolvedPath = path.relative(cwd, resolvedPath);
        try {
            if (fs.statSync(resolvedPath).isDirectory) {
                let index = path.posix.join(resolvedPath, 'index.html');
                if (fs.existsSync(index)) {
                    resolvedPath = index;
                }
            }
        } catch(_) {
            // Use path as is if it can't be accessed
        }
        callback({
            path: resolvedPath,
        });
    });
}

function createWindow() {
    log.info("=================================================")
    log.info('Enter CreateWindow()');
    interceptFileProtocol();

    mainWindow = new BrowserWindow({
        height: 1000,
        icon: path.join(__dirname, '../public/winml_icon.ico'),
        width: 1200,
    });
    global.mainWindow = mainWindow;
    
    log.info("main window is created");
    let pageUrl;
    for (const arg of process.argv.slice(1)) {
        if (arg.includes('://')) {
            pageUrl = arg;
            break;
        }
    }
    if (pageUrl === undefined) {
        pageUrl = url.format({
            pathname: path.join(__dirname, '../build/'),
            protocol: 'file',
        });
    }

    log.info('loading URL ' + pageUrl);
    
    mainWindow.loadURL(pageUrl);

    if (process.argv.includes('--dev-tools')) {
        mainWindow.webContents.openDevTools();
    }

    mainWindow.on('closed', () => {
        mainWindow = null;
        log.info("main windows is closed.")
        log.info("=================================================")
    });

    log.info('Exit CreateWindow()');
}

app.on('ready', createWindow);

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

app.on('activate', () => {
    if (mainWindow === null) {
        createWindow();
    }
});

ipcMain.on('show-about-window', () => {
    showAboutDialog();   
})

var pkg = {
    get package (){
        var package; 
        if (!package) {
            var appPath = app.getAppPath();
            var file = appPath + '/build/pkginfo.json';
            // if (fs.existsSync(file)) {
            try {
                var data = fs.readFileSync(file);
                package = JSON.parse(data);
                package.date = new Date(fs.statSync(file).mtime);
            }
            catch(_)
            {
                // package.commit = None;
                // package.date = None;
            }
        }    
        return package;
    }    
}

function showAboutDialog() {
    var details = [];

    details.push('Version: ' + app.getVersion() + ' (pre-release)');
    if (pkg.package){
        var commit = pkg.package.commit; 
        var date = pkg.package.date;

        details.push('Commit: ' + commit);
        if (date) {
            details.push('Date: ' + (date.getMonth() + 1).toString() + '/' + date.getDate() + '/' + date.getFullYear())
        }
    }

    var aboutDialogOptions = {
        detail: details.join('\n'),
        icon: path.join(__dirname, '../public/winml_icon.ico'),
        message: app.getName(),
        title: 'About'
    };

    dialog.showMessageBox(mainWindow, aboutDialogOptions);    
}
