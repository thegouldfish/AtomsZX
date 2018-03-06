#
# Basic script for compiling ZX spectrum code.
#
# Setup to support Build, Clean and Rebuild.
#
param
(	
    [String]$Action = "Build",
    [String]$SourceLocation = "",
    [String]$OutFolder = "",
    [String]$AssemblyName = "",
    [String]$ObjectFolder = "",
    [String]$Flavour = "Release",
    [String]$Z88dkPath = "c:\z88dk"
)

if(-not $Action)
{
    Write-Host "No Action"
    exit -1
}

if(-not $SourceLocation)
{
    Write-Host "No Source Location"
    exit -1
}

if(-not $OutFolder)
{
    Write-Host "No out folder"
    exit -1
}

if(-not $AssemblyName)
{
    Write-Host "No AssemblyName"
    exit -1
}

if(-not $ObjectFolder)
{
    Write-Host "No ObjectFolder"
    exit -1
}

if(-not $Z88dkPath)
{
    Write-Host "No Z88dkPath"
    exit -1
}



$Z88dkBin = "$Z88dkPath\bin"

$contains = $env:Path -contains $Z88dkBin
if(-not $contains)
{
    $env:Path = "$($env:Path);$Z88dkBin"
}

Write-Host "--== ZCC Compiler Script ==--"
Write-Host "Action: $Action"
Write-Host "SourceLocation: $SourceLocation"
Write-Host "OutFolder: $OutFolder"
Write-Host "AssemblyName: $AssemblyName"
Write-Host "ObjectFolder: $ObjectFolder"
Write-Host "Flavour: $Flavour"
Write-Host "Z88dkPath: $Z88dkPath"







function BuildCode()
{
    Write-Host "Compiling Build"
    $fileList = @("+zx", "-create-app")

    $list = Get-ChildItem -Path $SourceLocation -Filter "*.c" -Recurse

    $loadingScreen = "$SourceLocation\screen.scr"
    $loadingExisits = Test-Path $loadingScreen


    $exists = Test-Path $ObjectFolder
    if(-not $exists)
    {
        New-Item -ItemType directory -Path $ObjectFolder
    }


    if($Flavour -eq "Release")
    {
        $fileList += "-O3"
        $fileList += "-Cz+zx"
        $fileList += "-Cz--screen" 
        $fileList += "-Czscreen.scr"
    }
    else
    {
        $fileList += "-DDEBUG"        
    }

    $fileList += "-lndos"

    $fileList += "-o"
    $fileList += "$AssemblyName"


    
    foreach($item in $list)
    {
        if($item.Length -gt 0)
        {
            $fileList += $item.FullName
        }
    }
    
    Write-Host "zcc.exe "@fileList
    #$result = & zcc.exe +zx -create-app @fileList | Out-String

    $buildArgs = $fileList -join " "
    $pinfo = New-Object System.Diagnostics.ProcessStartInfo
    $pinfo.FileName = "zcc.exe"
    $pinfo.RedirectStandardError = $true
    $pinfo.RedirectStandardOutput = $true
    $pinfo.UseShellExecute = $false
    $pinfo.Arguments = $buildArgs
    $pinfo.WorkingDirectory = $ObjectFolder
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $pinfo
    $p.Start() | Out-Null
    $p.WaitForExit()
    $stdout = $p.StandardOutput.ReadToEnd()
    $stderr = $p.StandardError.ReadToEnd()
    
    $LASTEXITCODE = $p.ExitCode



    Write-Host $stdout

    # Parse the result for errors!

    $lines = $stderr -split "`n"

    foreach($line in $lines)
    {
        # sccz80:"E:\Programming\ZxSpectrum\Atoms\Atoms\main.c" L:43 Error:#27:Missing Open Parenthesis
        # <file_name>(row,column): error: <text> 

        if($line.Contains("Error:#") -or $line.Contains("Warning:#"))
        {
            $nameParts = $line -split "`""
            $name = $nameParts[1]

            $LineNumPos = $line.IndexOf("L:")
            $LineNum = $line.Substring($LineNumPos + 2)
            $LineNumPos = $LineNum.IndexOf(" ")
            $LineNum = $LineNum.Substring(0,$LineNumPos).Trim()


            if($line.Contains("Error:#"))
            {
                $errorParts = $line -split("Error:")
                $error = $errorParts[1]

                Write-Host "$name($LineNum,0): error: $error"
            }
            else
            {
                $warningParts = $line -split("Warning:")
                $warning = $warningParts[1]

                Write-Host "$name($LineNum,0): warning: $warning"
            }
        }
        else
        {
            if($line.Contains("Error at file"))
            {
                Write-Host $line
            }
        }
    }

    



    $fileList = Get-ChildItem -Path $ObjectFolder -Filter "*.tap"
    
    $OutFolder = "$OutFolder$Flavour"

    $exists = Test-Path $OutFolder
    if(-not $exists)
    {
        New-Item -ItemType directory -Path $OutFolder | Out-Null
    }

    if(Test-Path "$OutFolder\$AssemblyName.tap")
    {
        Remove-Item "$OutFolder\$AssemblyName.tap"
    }

    if($fileList -and $fileList.Length -gt 0)
    {
        Move-Item -Path $fileList[0].FullName -Destination "$OutFolder\$AssemblyName.tap"
        Write-Host "Made: $OutFolder\$AssemblyName.tap"
        $details = dir $ObjectFolder\$AssemblyName
        $size = $details.Length

        Write-Host "File size $("{0:N2}" -f ($size / 1024))KB $($size)B"
    }

    return $p.ExitCode
}


function CleanCode()
{
    Write-Host "Cleaning build"
    Get-ChildItem -path $ObjectFolder -Filter "*.bin" | ForEach-Object { $_.Delete() }
    Get-ChildItem -path $ObjectFolder -Filter "*.tap" | ForEach-Object { $_.Delete() }
    Get-ChildItem -path $ObjectFolder -Filter "*.def" | ForEach-Object { $_.Delete() }

    Get-ChildItem -path $OutFolder -Filter "*.tap" | ForEach-Object { $_.Delete() }
}


Try
{
    switch( $Action.ToLower() )
    {
        "build"
        {
            $exit = BuildCode
            Write-Host "Build Compete ($exit)"
            Exit $exit
        }

        "clean"
        {
            CleanCode
            Write-Host "Clean Compete (0)"
            Exit 0
        }


        "rebuild"
        {
            CleanCode
            $exit = BuildCode
            Write-Host "Rebuild Compete ($exit)"
            Exit $exit
        }   
    }
}
Catch
{
    Write-Host "Something went wrong: $_"
    Exit -1
}