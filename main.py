from ctypes import *
import pathlib
import PySimpleGUI as sg
import PIL.Image
import io
import os

imageDisplaySize = (500,500)

# Identical layout to the c++ struct, allowing for easy conversion
class FILE(Structure) :
    _fields_ = [("name", c_char*9),
                ("length", c_int),
                ("numBlocks",c_int),
                ("blockIDs",c_int*256)]

fileArrType = FILE*256

lib = cdll.LoadLibrary("pylib.so")
lib.getBlockUsageMap.restype = c_char_p
lib.getBytes.restype = c_void_p

def refreshFiles():
    # The c++ function returns a pointer to an array of file data structs
    print(lib.getBlockUsageMap().decode("utf-8"))
    lib.readFATEntries.restype = c_void_p
    files = fileArrType.from_address(lib.readFATEntries())
    fileNames = []
    fileDict = {}
    for i in files:
        if len(i.name) == 0 :
            break
        print("file name: ", i.name.decode())
        print("file size: ", i.length, "bytes")
        print("number of blocks: ", i.numBlocks, '\n')
        fileNames.append(i.name.decode())
        fileDict[i.name.decode()] = i
    return fileNames, fileDict

fileNames, fileDict = refreshFiles()

uploadPath = "code.txt"
uploadName = "test.txt"
lib.newFile.argtypes = [c_char_p, c_char_p]


#lib.extractFile(files[3])
unknown_format_layout = [
    [
        sg.Push(),
        sg.Text("This file has an unknown format \n Do you want to open it as a text file anyway?",justification='centre'),
        sg.Push()
    ],[
        sg.Push(),
        sg.Button("Open as text", key="forceOpenText"),
        sg.Push()
    ],[
        sg.Push(),
        sg.Text("Warning! Most file types will break horribly when viewed like this", justification='centre',text_color="red"),
        sg.Push()
    ]
]

preview_layout = [
    [
        sg.Text("Current file:",font=("",20,'')),
        sg.Text("",key="fileName",font=("",20,''))
    ],[
        sg.VPush()
    ],
    [
        sg.Push(),
        sg.Multiline("",write_only=False,key="textPreview", disabled=True,expand_y=True,visible=False,expand_x=True, size=(78, 40), pad=0),
        sg.Image(visible=False,key="imagePreview",),
        sg.Frame(title="",layout=unknown_format_layout,visible=False,expand_x=True,key="unknownFormatBox"),
        sg.Push()
    ],[
        sg.VPush()
    ],[
        sg.Button("Extract", key="ExtractCurrent", visible=True),
    ]
]

sidebar_layout = [
    [
        sg.Text("Available files:",font=("",20,''))
    ],[
        sg.Listbox(values=fileNames,expand_y=True,size=(15,20), expand_x=True, key="fileSelect", select_mode="LISTBOX_SELECT_MODE_SINGLE"),
    ],[
        sg.Button("Upload",key="openUploadMenu",expand_x=True)
    ]
]

main_layout = [
    [
        sg.Column(sidebar_layout,expand_y=True),
        sg.VerticalSeparator(),
        sg.Column(preview_layout, expand_y=True, size=(imageDisplaySize[0],imageDisplaySize[1]+25),expand_x=True)
    ]
]

window = sg.Window("File extractor", main_layout)
window.finalize()

currentFile = None
currentFileName = ""
currentMemLoc = 0
isUploadMenuOpen = False
while True:
    event, values = window.read(10)
    if event == sg.WIN_CLOSED:
        break
    selectedFile = window["fileSelect"].get()
    if len(selectedFile) != 0 and currentFileName != selectedFile[0]:
        window["ExtractCurrent"].update(visible=True)
        if currentMemLoc != 0:
            #lib.freeBytes(currentMemLoc)
            pass
        currentFileName = selectedFile[0]
        window["fileName"].update(currentFileName)
        
        # Disable all prieview elements, then re-enable the relevant one
        window["imagePreview"].update(visible=False)
        window["textPreview"].update(visible=False)
        window["unknownFormatBox"].update(visible=False)

        currentFile = fileDict[currentFileName]
        # text file handling
        if (currentFileName[-4:] == ".txt"):
            currentMemLoc = lib.getBytes(currentFile)
            fileStoreType = c_char*currentFile.length
            # Slicing the array converts it to a normal python array
            fileText = fileStoreType.from_address(currentMemLoc)[:].decode()

            window["textPreview"].update(fileText,visible=True)

        # image (png) file handling
        elif (currentFileName[-4:] == ".png"):
            currentMemLoc = lib.getBytes(currentFile)
            fileStoreType = c_char*currentFile.length
            # Slicing the array converts it to a normal python array
            fileData = fileStoreType.from_address(currentMemLoc)[:]

            # Resizing to fit in pane, taken from https://github.com/PySimpleGUI/PySimpleGUI/blob/master/DemoPrograms/Demo_Image_Elem_Image_Viewer_PIL_Based.py
            img = PIL.Image.open(io.BytesIO(fileData))
            curWidth, curHeight = img.size
            newWidth = imageDisplaySize[0]
            newHeight = imageDisplaySize[1]
            scaleFactor = min(newHeight/curHeight, newWidth/curWidth)
            img = img.resize((int(curWidth*scaleFactor), int(curHeight*scaleFactor)), PIL.Image.BILINEAR)

            bio = io.BytesIO()
            img.save(bio, format="PNG")
            del img
            fileData = bio.getvalue()

            window["imagePreview"].update(data=fileData,visible=True)

        # Unknown file format            
        else:
            window["unknownFormatBox"].update(visible=True)
        
        print(selectedFile[0])

    if "ExtractCurrent" in event:
        #print("pressed the button!")
        if currentFile == None:
            print("No selected file to extract")
        else:
            lib.extractFile(currentFile)

    if "forceOpenText" in event:
        currentMemLoc = lib.getBytes(currentFile)
        fileStoreType = c_char*currentFile.length
        # Slicing the array converts it to a normal python array
        fileText = fileStoreType.from_address(currentMemLoc)[:].decode()

        window["textPreview"].update(fileText,visible=True)
        window["unknownFormatBox"].update(visible=False)
    
    if "openUploadMenu" in event:
        isUploadMenuOpen = True
        currentDir = os.getcwd()
        localFiles = []
        localDirs = []
        fileSizes = []
        # All the files in the current directory
        with os.scandir(currentDir) as it:
            for entry in it:
                #print(entry.stat())
                if entry.is_file():
                    localFiles.append(entry.name)
                    fileSizes.append(entry.stat().st_size)
                if entry.is_dir():
                    localDirs.append(entry.name)
                    
        # Layouts aren't reusable, so they have to be redefined every time
        name_error_layout = [
            [
                sg.Text("", justification="centre", key="nameErrorText")
            ],[
                sg.InputText(size=(8,1), key="newFileName")
            ]
        ]
        upload_dirlist_layout = [
            [
                sg.Text("Available directories:"),
            ],[
                sg.Listbox(values=localDirs,size=(None, 20), key="dirSelect"),
            ],[
                sg.Button("Open",key="openDir",expand_x=True),
                sg.Button("Back",key="backDir", expand_x=True)
            ]
        ]
        upload_filelist_layout = [
            [
                sg.Text("Available files:"),
            ],[
                sg.Listbox(values=localFiles,size=(None, 20), key="uploadSelect"),
            ],[
                sg.Button("Upload",key="writeFile",expand_x=True)
            ]
        ]
        upload_select_layout = [
            [
                sg.Text("Choose a file to upload:")
            ],[
                sg.InputText(currentDir,key="fullPath",expand_x=True),
                sg.Button("Go", key="jumpDir")
            ],[
                sg.Column(upload_dirlist_layout),
                sg.Column(upload_filelist_layout),
            ],[
                sg.Frame(title="",layout=name_error_layout, key="nameErrorBox", visible= False)
            ]
        ]
        upload_window = sg.Window("File chooser", upload_select_layout)
        upload_window.finalize()

    # The upload menu code
    if isUploadMenuOpen:
        
        def refreshFileList():
            localFiles.clear()
            localDirs.clear()
            fileSizes.clear()
            with os.scandir(currentDir) as it:
                for entry in it:
                    if entry.is_file():
                        localFiles.append(entry.name)
                        fileSizes.append(entry.stat().st_size)
                    if entry.is_dir():
                        localDirs.append(entry.name)
            upload_window["dirSelect"].update(localDirs)
            upload_window["uploadSelect"].update(localFiles)
            upload_window["fullPath"].update(currentDir)

        event, values = upload_window.read(10)

        if event == sg.WIN_CLOSED:
            isUploadMenuOpen = False
            upload_window.close()
            continue

        if "writeFile" in event and len(upload_window["uploadSelect"].get()) >= 1:
            currentFileName = upload_window["uploadSelect"].get()[0]
            shortenedFileName = upload_window["newFileName"].get()
            uploadPath = currentDir + currentFileName

            if (len(shortenedFileName) == 0):
                shortenedFileName = currentFileName
            if len(shortenedFileName) > 8:
                upload_window["nameErrorBox"].update(visible=True)
                upload_window["nameErrorText"].update("This file name is too long\nPlease provide a name of up to 8 characters")
                continue
            uploadName = shortenedFileName
            
            if (not uploadName in fileNames):
                # Null padding
                uploadName += (8 - len(uploadName))*'\0'

                lib.newFile(c_char_p(uploadPath.encode()), c_char_p(uploadName.encode()))
                fileNames, fileDict = refreshFiles()
                window["fileSelect"].update(values=fileNames)
                isUploadMenuOpen = False
                upload_window.close()
            else:
                upload_window["nameErrorBox"].update(visible=True)
                upload_window["nameErrorText"].update("This file name is already in use\nPlease provide another one")
        if "openDir" in event and len(upload_window["dirSelect"].get()) >= 1:
            if currentDir[-1] != "/": currentDir += "/"
            currentDir += upload_window["dirSelect"].get()[0]
            refreshFileList()
        if "backDir" in event and currentDir != "/":
            currentDir = currentDir[:-1]
            lastSlash = currentDir.rfind("/")
            currentDir = currentDir[:lastSlash]
            if currentDir == "": currentDir = "/"
            refreshFileList()
        if "jumpDir" in event:
            oldDir = currentDir
            try:
                currentDir = upload_window["fullPath"].get()
                refreshFileList()
            except:
                currentDir = oldDir
                pass
            

window.close()