'Enable Strict Mode
Strict

'Import modules
Import mojo
Import autofit
Import box2d
Import mjc.box2d.box2dWorld
Import fontmachine
'Import skn3.screenres

#If TARGET = "glfw"
	#GLFW_WINDOW_TITLE="Rebound"
	#GLFW_WINDOW_WIDTH=1280            'Set this and HEIGHT to 0 for a 'windowless' glfw app
	#GLFW_WINDOW_HEIGHT=720              
	#GLFW_WINDOW_RESIZABLE=True
	#GLFW_WINDOW_FULLSCREEN=False
	
	Import "native/newwindow.cpp"
	Extern
	Function launchBrowser:Void(address:String, windowName:String) = "browser::launchBrowser"
	Public
	
#Elseif TARGET = "html5"

	Import "native/newwindow.js"
	Extern
	Function launchBrowser:Void(address:String, windowName:String)
	Public
	
#Elseif TARGET = "android"

	#ANDROID_SCREEN_ORIENTATION="landscape" 
	#ANDROID_APP_LABEL="Rebound"
	#ANDROID_APP_PACKAGE="com.mjcamerer.rebound"
	#ANDROID_VERSION_CODE="6"
	#ANDROID_VERSION_NAME="1.05"
	
	#ANDROID_SIGN_APP=true
	#ANDROID_KEY_STORE="XXXXXXXX"
	#ANDROID_KEY_ALIAS="XXXXXXXX"
	#ANDROID_KEY_STORE_PASSWORD="XXXXXXXX"
	#ANDROID_KEY_ALIAS_PASSWORD="XXXXXXXX"
	
#Endif

Global currentVersionCode:Int = 105
Global versionCode:Int = 0

'Main Function
Function Main:Int()
	New Game()
	Return 0
End Function

'Game Class
Class Game Extends App	

	'Physics Fields
	Field world:Box2D_World
	Field ground:Entity
	Field lastFrame:Int
	Field debug:Bool
	
	'Image Fields
	Field img_player:Image
	Field img_mainMenu:Image
	Field img_stageSelect:Image
	Field img_levelComplete:Image
	Field img_level:Image
	Field img_level1:Image
	Field img_level2:Image
	Field img_level3:Image
	Field img_level4:Image
	Field img_level5:Image
	Field img_level6:Image
	Field img_level7:Image
	Field img_level8:Image
	Field img_level9:Image
	Field img_level10:Image
	Field img_level11:Image
	Field img_level12:Image
	Field img_level13:Image
	Field img_level14:Image
	Field img_level15:Image
	Field img_level16:Image
	Field img_level17:Image
	Field img_level18:Image
	Field img_level19:Image
	Field img_level20:Image
	Field img_level21:Image
	Field img_level22:Image
	Field img_level23:Image
	Field img_level24:Image
	Field img_redo:Image
	Field img_menu:Image
	Field img_exit:Image
	Field img_starFull:Image
	Field img_starFullLarge:Image
	Field img_barrierV:Image
	Field img_barrierH:Image
	Field img_cross1:Image
	Field img_cube1:Image
	Field img_levelH:Image
	Field img_starFullH:Image
	Field img_rightArrowH:Image
	Field img_leftArrowH:Image
	Field img_menuH1:Image
	Field img_optionsH1:Image
	Field img_playH:Image
	Field img_tutorialH:Image
	Field img_optionsH2:Image
	Field img_menuH2:Image
	Field img_redoH:Image
	Field img_menuH3:Image
	Field img_nextH:Image
	Field img_twitterH:Image
	Field img_arrowR:Image
	Field img_arrowL:Image
	Field img_options:Image
	Field img_res1H:Image
	Field img_res2H:Image
	Field img_res3H:Image
	Field img_res1:Image
	Field img_windowedH:Image
	Field img_fullscreenH:Image
	Field img_creditsH:Image
	Field img_exitGameH:Image
	Field img_returnH:Image
	Field img_credits1:Image
	Field img_credits2:Image
	Field img_credits3:Image
	Field img_credits4:Image
	Field img_credits5:Image
	Field img_credits6:Image
	Field img_stage2:Image
	Field img_stage3:Image
	Field img_completed:Image
	Field img_androidOptions:Image
	Field img_on:Image
	Field img_off:Image
	
	'Font Fields
	Field fnt_stageFont:BitmapFont
	Field fnt_stageSelectFont:BitmapFont
	Field fnt_stageSelectFontH:BitmapFont
	Field fnt_72Font:BitmapFont
	Field fnt_54Font:BitmapFont
	Field fnt_timeFont:BitmapFont
	
	'Player Fields
	Global playerColliding:Bool = False
	Field player:Entity
	Field playerX:Float
	Field playerY:Float
	Field playerFriction:Int
	
	'Tutorial Fields
	Field tutorial:Bool
	Field tutStep:Int
	Field tutTime:Int
	Field tutWait:Int
	
	'Options Fields
	Field options:Bool
	Field fullscreen:Bool
	Field resX:Int
	Field resY:Int
	
	'Credits
	Field credits:Bool
	Field creditStep:Int
	Field creditStart:Int
	
	'Game Fields
	Global sensorColliding:Bool = False
	Field level:Int
	Field stage:Int
	Field maxAngularVelocity:Float
	Field hasWon:Bool
	Field levelStartTimer:Int
	Field finalTime:Float
	Field fTime:String
	Field tStars:Int
	Field exitX:Float
	Field exitY:Float
	Field barrierList:List<Barrier>
	Field touchDelayStart:Int
	Field touchDelayTime:Int
	Field musicPlaying:Bool
	
	'Data Fields
	Field gameData:GameData
	
	'UI Fields
	Field main_menu:Bool
	Field isPlaying:Bool
	Field levelComplete:Bool
	Field selector:Int
	Field oldSelector:Int
	Field mX:Float
	Field mY:Float
	Field gamepad:Bool = False
	Field gameProgress:Int
	Field gameProgressTimer:Int
	
	'Runs on app creation
	Method OnCreate:Int()
	
		'Set Virtual Resolution
		SetVirtualDisplay(1920, 1080)
		
		'Load Images
		img_player = LoadImage("Graphics/Player/Player.png",, Image.MidHandle)
		img_mainMenu = LoadImage("Graphics/UI/Main_Menu.png")
		img_stageSelect = LoadImage("Graphics/UI/Stage_Select.png")
		img_levelComplete = LoadImage("Graphics/UI/Level_Complete.png",, Image.MidHandle)
		img_level = LoadImage("Graphics/Levels/Level12.png")
		'img_level1 = LoadImage("Graphics/Levels/Level1.png")
		'img_level2 = LoadImage("Graphics/Levels/Level2.png")
		'img_level3 = LoadImage("Graphics/Levels/Level3.png")
		'img_level4 = LoadImage("Graphics/Levels/Level4.png")
		'img_level5 = LoadImage("Graphics/Levels/Level5.png")
		'img_level6 = LoadImage("Graphics/Levels/Level6.png")
		'img_level7 = LoadImage("Graphics/Levels/Level7.png")
		'img_level8 = LoadImage("Graphics/Levels/Level8.png")
		'img_level9 = LoadImage("Graphics/Levels/Level9.png")
		'img_level10 = LoadImage("Graphics/Levels/Level10.png")
		'img_level11 = LoadImage("Graphics/Levels/Level11.png")
		'img_level12 = LoadImage("Graphics/Levels/Level12.png")
		'img_level13 = LoadImage("Graphics/Levels/Level13.png")
		'img_level14 = LoadImage("Graphics/Levels/Level14.png")
		'img_level15 = LoadImage("Graphics/Levels/Level15.png")
		'img_level16 = LoadImage("Graphics/Levels/Level16.png")
		'img_level17 = LoadImage("Graphics/Levels/Level17.png")
		'img_level18 = LoadImage("Graphics/Levels/Level18.png")
		'img_level19 = LoadImage("Graphics/Levels/Level19.png")
		'img_level20 = LoadImage("Graphics/Levels/Level20.png")
		'img_level21 = LoadImage("Graphics/Levels/Level21.png")
		'img_level22 = LoadImage("Graphics/Levels/Level22.png")
		'img_level23 = LoadImage("Graphics/Levels/Level23.png")
		'img_level24 = LoadImage("Graphics/Levels/Level24.png")
		img_redo = LoadImage("Graphics/UI/Redo.png")
		img_menu = LoadImage("Graphics/UI/Menu.png")
		img_exit = LoadImage("Graphics/UI/Exit.png")
		img_starFull = LoadImage("Graphics/UI/Star_Full.png")
		img_starFullLarge = LoadImage("Graphics/UI/Star_Full_Large.png")
		img_levelH = LoadImage("Graphics/UI/Level_Highlighted.png")
		img_starFullH = LoadImage("Graphics/UI/Star_Full_Highlighted.png")
		img_rightArrowH = LoadImage("Graphics/UI/Right_Arrow_Highlighted.png")
		img_leftArrowH = LoadImage("Graphics/UI/Left_Arrow_Highlighted.png")
		img_menuH1 = LoadImage("Graphics/UI/Menu_Highlighted.png")
		img_optionsH1 = LoadImage("Graphics/UI/Options_Highlighted.png")
		img_playH = LoadImage("Graphics/UI/Play_Highlighted.png")
		img_tutorialH = LoadImage("Graphics/UI/Tutorial_Highlighted.png")
		img_optionsH2 = LoadImage("Graphics/UI/Options_Highlighted2.png")
		img_menuH2 = LoadImage("Graphics/UI/Menu_Highlighted2.png")
		img_menuH3 = LoadImage("Graphics/UI/Menu_Highlighted3.png")
		img_nextH = LoadImage("Graphics/UI/Next_Highlighted.png")
		img_twitterH = LoadImage("Graphics/UI/Twitter_Highlighted.png")
		img_redoH = LoadImage("Graphics/UI/Redo_Highlighted.png")
		img_barrierV = LoadImage("Graphics/Objects/Barrier_V.png",, Image.MidHandle)
		img_barrierH = LoadImage("Graphics/Objects/Barrier_H.png",, Image.MidHandle)
		img_cross1 = LoadImage("Graphics/Objects/Cross1.png",, Image.MidHandle)
		img_cube1 = LoadImage("Graphics/Objects/Cube1.png",, Image.MidHandle)
		img_options = LoadImage("Graphics/UI/Options.png",, Image.MidHandle)
		img_res1H = LoadImage("Graphics/UI/Res1H.png")
		img_res2H = LoadImage("Graphics/UI/Res2H.png")
		img_res3H = LoadImage("Graphics/UI/Res3H.png")
		img_windowedH = LoadImage("Graphics/UI/WindowedH.png")
		img_fullscreenH = LoadImage("Graphics/UI/FullscreenH.png")
		img_creditsH = LoadImage("Graphics/UI/CreditsH.png")
		img_exitGameH = LoadImage("Graphics/UI/Exit_GameH.png")
		img_returnH = LoadImage("Graphics/UI/ReturnH.png")
		'img_credits1 = LoadImage("Graphics/UI/Credits1.png")
		'img_credits2 = LoadImage("Graphics/UI/Credits2.png")
		'img_credits3 = LoadImage("Graphics/UI/Credits3.png")
		'img_credits4 = LoadImage("Graphics/UI/Credits4.png")
		'img_credits5 = LoadImage("Graphics/UI/Credits5.png")
		'img_credits6 = LoadImage("Graphics/UI/Credits6.png")
		img_stage2 = LoadImage("Graphics/UI/Stage2.png")
		img_stage3 = LoadImage("Graphics/UI/Stage3.png")
		img_completed = LoadImage("Graphics/UI/Complete.png")
		img_androidOptions = LoadImage("Graphics/UI/Options_Android_Html.png",, Image.MidHandle)
		img_on = LoadImage("Graphics/UI/On.png")
		img_off = LoadImage("Graphics/UI/Off.png")
		
		'Load Fonts
		fnt_stageFont = New BitmapFont("Graphics/Fonts/Stage_Font.txt", False)
		fnt_stageSelectFont = New BitmapFont("Graphics/Fonts/Stage_Select_Font.txt", False)
		fnt_stageSelectFontH = New BitmapFont("Graphics/Fonts/Stage_Select_FontH.txt", False)
		fnt_72Font = New BitmapFont("Graphics/Fonts/72Font.txt", False)
		fnt_54Font = New BitmapFont("Graphics/Fonts/54Font.txt", False)
		fnt_timeFont = New BitmapFont("Graphics/Fonts/Time_Font.txt", False)
		
		
		'Create World
		debug = False
		world = New Box2D_World(0.0, 10.0, 64.0, debug)
		
		'Tutorial
		tutorial = False
		tutStep = 0
		
		'Options
		options = False
		fullscreen = False
		resX = 1280
		resY = 720
		
		'Credits
		credits = False
		creditStep = 1
		
		'Game
		level = 1
		stage = 1
		hasWon = False
		levelStartTimer = 0
		finalTime = 0.0
		barrierList = New List<Barrier>
		touchDelayStart = 0
		touchDelayTime = 90
		
		'UI
		main_menu = True
		isPlaying = False
		levelComplete = False
		oldSelector = 9999
		selector = 9999
		mX = VTouchX()
		mY = VTouchY()
		gamepad = False
		gameProgress = 0
		
		'Game Data
		gameData = New GameData()
		Local saveData:String = LoadState()
		Local mus:Int = 1
		If saveData = ""
			mus = 1
			SaveState(gameData.SaveString(gameProgress, mus))
			'Print "NEW GAME"
		Else
			Local s3:String[] = saveData.Split(",")
			gameData.LoadString(saveData)
			versionCode = Int(s3[s3.Length - 3])
			
			If versionCode = currentVersionCode
				Local s:String[] = saveData.Split(",")
				gameProgress = Int(s[s.Length() - 2])
				mus = Int(s[s.Length() - 1])
				Print "LOADED GAME DATA"
			Else
				If versionCode < 105
					
					Local s:String[] = saveData.Split(",")
					gameProgress = Int(s[s.Length() - 1])
					SaveState(gameData.SaveString(gameProgress, 1))
					saveData = ""
					saveData = LoadState()
					gameData = New GameData()
					gameData.LoadString(saveData)
					Local s5:String[] = saveData.Split(",")
					gameProgress = Int(s5[s5.Length() - 2])
					mus = Int(s5[s5.Length() - 1])
					Print "LOADED NEW VERSION GAME DATA"
					
				Else
				
					Local s:String[] = saveData.Split(",")
					Print "length: " + s.Length()
					gameProgress = Int(s[s.Length() - 1])
					'mus = Int(s[s.Length() - 1])
					Print "gameProgress: " + gameProgress
					SaveState(gameData.SaveString(gameProgress, mus))
					saveData = ""
					saveData = LoadState()
					gameData = New GameData()
					gameData.LoadString(saveData)
					Local s5:String[] = saveData.Split(",")
					gameProgress = Int(s5[s5.Length() - 2])
					mus = Int(s5[s5.Length() - 1])
					Print "LOADED NEW VERSION GAME DATA"
					
				EndIf
			EndIf
		Endif
		gameData.stage[stage - 1].level[level - 1].unlocked = True
		If mus = 1
			musicPlaying = True
		Else
			musicPlaying = False
		EndIf
		'SaveState("")
		
		'Build Level
		BuildLevel(level)
		
		'Player
		#If TARGET = "android"
			playerFriction = 50000
		#Else
			playerFriction = 500000000000'50000000000000000000000000000
		#EndIf
		player = Self.world.CreateImageBox(Self.img_player, 400, 476, False, 0.89, playerFriction, 5000)
		'ResetPlayer(level)
		maxAngularVelocity = 7.5
		
		'Set Update Rate
		SetUpdateRate(0)
		ShowMouse()
		
		PlayMusic("Music/Ouroboros.ogg")
		If musicPlaying = False
			PauseMusic()
		EndIf
		
		'Return 0
		Return 0
		
	End Method
	
	'Runs when app is ready to update
	Method OnUpdate:Int()
	
		If isPlaying = True
		
			If tutorial = False
		
			'HTML5 Controls
			If KeyDown(KEY_D) Or KeyDown(KEY_RIGHT)
				player.body.SetAngularVelocity(player.body.GetAngularVelocity() + 0.1)
				If player.body.GetAngularVelocity() > maxAngularVelocity Then player.body.SetAngularVelocity(maxAngularVelocity)
			Endif
			If KeyDown(KEY_A) Or KeyDown(KEY_LEFT)
				player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
				If player.body.GetAngularVelocity() < -maxAngularVelocity Then player.body.SetAngularVelocity(-maxAngularVelocity)
			Endif
			
			'XBOX CONTROLLER SUPPORT TEST
			#If TARGET = "glfw"
				If gamepad = True
					If JoyZ() < -0.01 Or JoyDown(JOY_RIGHT)
						player.body.SetAngularVelocity(player.body.GetAngularVelocity() + 0.1)
						If player.body.GetAngularVelocity() > maxAngularVelocity Then player.body.SetAngularVelocity(maxAngularVelocity)
					Endif
					If JoyZ() > 0.01 Or JoyDown(JOY_LEFT)
						player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
						If player.body.GetAngularVelocity() < -maxAngularVelocity Then player.body.SetAngularVelocity(-maxAngularVelocity)
					Endif
					
					'Reset Level
					If JoyHit(JOY_X)
						ResetPlayer(level)
					Endif
					
					If JoyHit(JOY_B)
						isPlaying = False
						main_menu = False
						levelComplete = False
						selector = ((stage - 1)* 8) + level
					Endif
					
				EndIf
			#End If
			
			
			'Android Controls
			#If TARGET = "android"
				If Millisecs() >= (touchDelayStart + touchDelayTime)
					If TouchDown()
						If VTouchX() >= 960.0
							player.body.SetAngularVelocity(player.body.GetAngularVelocity() + 0.1)
							If player.body.GetAngularVelocity() > maxAngularVelocity Then player.body.SetAngularVelocity(maxAngularVelocity)
						Elseif VTouchX() <= 960.0
							If VTouchX() >= 40.0 And VTouchX() <= 141.0
								If VTouchY() >= 25.0 And VTouchY() <= 133.0
								Else
									player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
									If player.body.GetAngularVelocity() < -maxAngularVelocity Then player.body.SetAngularVelocity(-maxAngularVelocity)
								Endif
							Else
								player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
								If player.body.GetAngularVelocity() < -maxAngularVelocity Then player.body.SetAngularVelocity(-maxAngularVelocity)
							EndIf
						Endif
					Endif
				ENdIf
			#EndIf
			
			'Check Redo Hit
			If gamepad = False
				If VTouchX() >= 20.0 And VTouchX() <= 200.0
					If VTouchY() >= 20.0 And VTouchY() <= 200.0
						If TouchHit()
							'ResetPlayer(level)
							LoadLevel2(level)
							touchDelayStart = Millisecs()
						Endif
						selector = 1
					Else
						selector = 9999
					Endif
				Else
					selector = 9999
				Endif
				If KeyHit(KEY_R)
					'ResetPlayer(level)
					LoadLevel(level)
				Endif
				
				'Check Menu Button
				If selector = 9999
					If VTouchX() >= 1715.0 And VTouchX() <= 1885.0
						If VTouchY() >= 25.0 And VTouchY() <= 80.0
							If TouchHit()
								levelComplete = False
								isPlaying = False
							Endif
							selector = 2
						Else
							selector = 9999
						Endif
					Else
						selector = 9999
					Endif
				Endif
			EndIf
			
			'Check Win
			If sensorColliding
				levelComplete = True
				finalTime = (Millisecs() - levelStartTimer)
				'Local st:String = String((Millisecs() - levelStartTimer)/1000) +  "."
				'Local mm:String = String(finalTime)
				'Local len:Int = mm.Length()
				'st = st + mm[(len - 3)..(len - 2)]
				'fTime = finalTime'st
				Local tt:Int = finalTime
				Local st:String = String((tt)/1000) +  "."
				Local mm:String = String(tt)
				Local len:Int = mm.Length()
				st = st + mm[(len - 3)..(len - 2)]
				fTime = st
				tStars = AssignStars(stage, level, finalTime)
				gameData.CompleteLevel(stage, level, finalTime)
				Local mus:Int = 0
				If musicPlaying = True
					mus = 1
				EndIf
				SaveState(gameData.SaveString(gameProgress, mus))
				isPlaying = False
				selector = 1
				
				
				'Draw best time
				'Local tt:Int = gameData.stage[stage - 1].level[level - 1].bestTime
				'Local st:String = String((tt)/1000) +  "."
				'Local mm:String = String(tt)
				'Local len:Int = mm.Length()
				'st = st + mm[(len - 3)..(len - 2)]
				'fnt_timeFont.DrawText(st , 1005, 453)
				
				'level = level + 1
				'If level > 5 Then level = 1
				'world.Clear()
				'BuildLevel(level)
				'ResetPlayer(level)
					
				'hasWon = True
			Endif
			
			
			'Update Barriers
			For Local b:Barrier = Eachin barrierList
				b.Update()
			Next
			
			'Update Physics
			'If hasWon = False
				'Self.world.Update()
			'Endif
			
			Elseif tutorial = true
			
				If tutStep = 0
				
					If Millisecs() >= tutTime + tutWait
						tutStep = 1
						tutTime = Millisecs()
					Endif
					
				Elseif tutStep = 1
				
					If Millisecs() >= tutTime + tutWait
						tutStep = 2
						tutTime = Millisecs()
					Endif
					
				Elseif tutStep = 2
				
					If Millisecs() >= tutTime + tutWait
						tutStep = 3
						tutTime = Millisecs()
					Endif
				
				Elseif tutStep = 3
				
					If Millisecs() >= tutTime + tutWait
						tutStep = 4
						tutTime = Millisecs()
						LoadLevel(level)
						touchDelayStart = Millisecs()
					Endif
					
				Elseif tutStep = 4
					
					#If TARGET = "glfw"
						If JoyZ() < -0.01 Or JoyDown(JOY_RIGHT)
							player.body.SetAngularVelocity(player.body.GetAngularVelocity() + 0.1)
							If player.body.GetAngularVelocity() > maxAngularVelocity 
								player.body.SetAngularVelocity(maxAngularVelocity)
								tutStep = 5
								tutTime = Millisecs()
							EndIf
						Endif
					#Endif
					
					#If TARGET = "android"
						If Millisecs() >= (touchDelayStart + touchDelayTime)
							If TouchDown()
								If VTouchX() >= 960.0
									player.body.SetAngularVelocity(player.body.GetAngularVelocity() + 0.1)
									If player.body.GetAngularVelocity() > maxAngularVelocity 
										player.body.SetAngularVelocity(maxAngularVelocity)
										tutStep = 5
										tutTime = Millisecs()
									EndIf
								Endif
							Endif
						Endif
					#EndIf
				
					If KeyDown(KEY_D) Or KeyDown(KEY_RIGHT)
						player.body.SetAngularVelocity(player.body.GetAngularVelocity() + 0.1)
						If player.body.GetAngularVelocity() > maxAngularVelocity 
							player.body.SetAngularVelocity(maxAngularVelocity)
							tutStep = 5
							tutTime = Millisecs()
						Endif
					Endif
					player.body.SetPosition(New b2Vec2(960.0/64.0, 640.0/64.0))
					
					
				Elseif tutStep = 5
				
					If Millisecs() >= tutTime + tutWait
						tutStep = 6
						img_level.Discard()
						img_level = LoadImage("Graphics/Levels/Level1.png")
						tutTime = Millisecs()
						ResetPlayer(level)
					Endif
					player.body.SetPosition(New b2Vec2(960.0/64.0, 640.0/64.0))
					
				Elseif tutStep = 6
					
					#If TARGET = "glfw"
						If JoyZ() > 0.01 Or JoyDown(JOY_LEFT)
							player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
							If player.body.GetAngularVelocity() < -maxAngularVelocity
								player.body.SetAngularVelocity(-maxAngularVelocity)
								tutStep = 7
								tutTime = Millisecs()
							EndIf
						Endif
					#Endif
					
					#If TARGET = "android"
						If Millisecs() >= (touchDelayStart + touchDelayTime)
							If TouchDown()
								If VTouchX() <= 960.0
									player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
									If player.body.GetAngularVelocity() < -maxAngularVelocity 
										player.body.SetAngularVelocity(-maxAngularVelocity)
										tutStep = 7
										tutTime = Millisecs()
									EndIf
								Endif
							Endif
						Endif
					#EndIf
					
					If KeyDown(KEY_A) Or KeyDown(KEY_LEFT)
						player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
						If player.body.GetAngularVelocity() < -maxAngularVelocity
							player.body.SetAngularVelocity(-maxAngularVelocity)
							tutStep = 7
							tutTime = Millisecs()
						EndIf
					Endif
					player.body.SetPosition(New b2Vec2(960.0/64.0, 640.0/64.0))
					
				Elseif tutStep = 7
					
					If Millisecs() >= tutTime + tutWait
						tutStep = 8
						tutTime = Millisecs()
						tutWait = 3500
						level = 2
						ResetPlayer(level)
						CreateExit(1700, 946)
					Endif
					player.body.SetPosition(New b2Vec2(960.0/64.0, 640.0/64.0))
					
				Elseif tutStep = 8
					
					If Millisecs() >= tutTime + tutWait
						tutStep = 9
						tutWait = 3500
						tutTime = Millisecs()
					Endif
					player.body.SetPosition(New b2Vec2(400.0/64.0, 593.0/64.0))
					
				Elseif tutStep = 9
					
					If Millisecs() >= tutTime + tutWait
						tutStep = 10
						tutTime = Millisecs()
						tutWait = 2500
					Endif
					player.body.SetPosition(New b2Vec2(400.0/64.0, 593.0/64.0))
					
				Elseif tutStep = 10
					
					If Millisecs() >= tutTime + tutWait
						tutStep = 11
						tutTime = Millisecs()
						tutWait = 4000
						BuildLevel(level)
						ResetPlayer(level)
						CreateExit(1700, 946)
					Endif
					player.body.SetPosition(New b2Vec2(400.0/64.0, 593.0/64.0))
					
				Elseif tutStep = 11
					
					If Millisecs() >= tutTime + tutWait
						tutStep = 12
						tutTime = Millisecs()
					Endif
					
					#If TARGET = "glfw"
					
						If JoyZ() > 0.01 Or JoyDown(JOY_LEFT)
							player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
							If player.body.GetAngularVelocity() < -maxAngularVelocity
								player.body.SetAngularVelocity(-maxAngularVelocity)
							EndIf
						Endif
						
						If JoyZ() < -0.01 Or JoyDown(JOY_RIGHT)
							player.body.SetAngularVelocity(player.body.GetAngularVelocity() + 0.1)
							If player.body.GetAngularVelocity() > maxAngularVelocity 
								player.body.SetAngularVelocity(maxAngularVelocity)
							EndIf
						Endif
						
					#Endif
					
					#If TARGET = "android"
						If Millisecs() >= (touchDelayStart + touchDelayTime)
							If TouchDown()
								If VTouchX() >= 960.0
									player.body.SetAngularVelocity(player.body.GetAngularVelocity() + 0.1)
									If player.body.GetAngularVelocity() > maxAngularVelocity Then player.body.SetAngularVelocity(maxAngularVelocity)
								Elseif VTouchX() <= 960.0
									If VTouchX() >= 40.0 And VTouchX() <= 141.0
										If VTouchY() >= 25.0 And VTouchY() <= 133.0
										Else
											player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
											If player.body.GetAngularVelocity() < -maxAngularVelocity Then player.body.SetAngularVelocity(-maxAngularVelocity)
										Endif
									Else
										player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
										If player.body.GetAngularVelocity() < -maxAngularVelocity Then player.body.SetAngularVelocity(-maxAngularVelocity)
									EndIf
								Endif
							Endif
						ENdIf
					#EndIf
					
					If KeyDown(KEY_A) Or KeyDown(KEY_LEFT)
						player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
						If player.body.GetAngularVelocity() < -maxAngularVelocity
							player.body.SetAngularVelocity(-maxAngularVelocity)
						EndIf
					Endif
					
					If KeyDown(KEY_D) Or KeyDown(KEY_RIGHT)
						player.body.SetAngularVelocity(player.body.GetAngularVelocity() + 0.1)
						If player.body.GetAngularVelocity() > maxAngularVelocity 
							player.body.SetAngularVelocity(maxAngularVelocity)
						Endif
					Endif
					
				Elseif tutStep = 12
					
					'If Millisecs() >= tutTime + tutWait
					'	tutStep = 13
					'	tutTime = Millisecs()
					'Endif
					
					#If TARGET = "glfw"
						If JoyHit(JOY_X)
							ResetPlayer(level)
							CreateExit(800, 930)
							tutStep = 13
							tutWait = Millisecs()
						Endif
					#EndIf
					
					If VTouchX() >= 20.0 And VTouchX() <= 200.0
						If VTouchY() >= 20.0 And VTouchY() <= 200.0
							If TouchHit()
								ResetPlayer(level)
								tutStep = 13
								tutWait = Millisecs()
								touchDelayStart = Millisecs()
							Endif
							selector = 1
						Else
							selector = 9999
						Endif
					Else
						selector = 9999
					Endif
					If KeyHit(KEY_R)
						ResetPlayer(level)
						CreateExit(800, 930)
						tutStep = 13
						tutWait = Millisecs()
					Endif
					
				Elseif tutStep = 13
					
					'If Millisecs() >= tutTime + tutWait
					'	tutStep = 14
					'	tutTime = Millisecs()
					'Endif
					
					#If TARGET = "glfw"
					
						If JoyZ() > 0.01 Or JoyDown(JOY_LEFT)
							player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
							If player.body.GetAngularVelocity() < -maxAngularVelocity
								player.body.SetAngularVelocity(-maxAngularVelocity)
							EndIf
						Endif
						
						If JoyZ() < -0.01 Or JoyDown(JOY_RIGHT)
							player.body.SetAngularVelocity(player.body.GetAngularVelocity() + 0.1)
							If player.body.GetAngularVelocity() > maxAngularVelocity 
								player.body.SetAngularVelocity(maxAngularVelocity)
							EndIf
						Endif
						
						If JoyHit(JOY_X)
							ResetPlayer(level)
						Endif
						
					#Endif
					
					#If TARGET = "android"
						If Millisecs() >= (touchDelayStart + touchDelayTime)
							If TouchDown()
								If VTouchX() >= 960.0
									player.body.SetAngularVelocity(player.body.GetAngularVelocity() + 0.1)
									If player.body.GetAngularVelocity() > maxAngularVelocity Then player.body.SetAngularVelocity(maxAngularVelocity)
								Elseif VTouchX() <= 960.0
									If VTouchX() >= 40.0 And VTouchX() <= 141.0
										If VTouchY() >= 25.0 And VTouchY() <= 133.0
										Else
											player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
											If player.body.GetAngularVelocity() < -maxAngularVelocity Then player.body.SetAngularVelocity(-maxAngularVelocity)
										Endif
									Else
										player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
										If player.body.GetAngularVelocity() < -maxAngularVelocity Then player.body.SetAngularVelocity(-maxAngularVelocity)
									EndIf
								Endif
							Endif
						ENdIf
					#EndIf
					
					If KeyDown(KEY_A) Or KeyDown(KEY_LEFT)
						player.body.SetAngularVelocity(player.body.GetAngularVelocity() - 0.1)
						If player.body.GetAngularVelocity() < -maxAngularVelocity
							player.body.SetAngularVelocity(-maxAngularVelocity)
						EndIf
					Endif
					
					If KeyDown(KEY_D) Or KeyDown(KEY_RIGHT)
						player.body.SetAngularVelocity(player.body.GetAngularVelocity() + 0.1)
						If player.body.GetAngularVelocity() > maxAngularVelocity 
							player.body.SetAngularVelocity(maxAngularVelocity)
						Endif
					Endif
					
					If VTouchX() >= 20.0 And VTouchX() <= 200.0
						If VTouchY() >= 20.0 And VTouchY() <= 200.0
							If TouchHit()
								ResetPlayer(level)
								touchDelayStart = Millisecs()
							Endif
							selector = 1
						Else
							selector = 9999
						Endif
					Else
						selector = 9999
					Endif
					
					If KeyHit(KEY_R)
						ResetPlayer(level)
					Endif
					
					If sensorColliding
						tutStep = 14
						tutWait = 2000
						tutTime = Millisecs()
						player.Kill()
						
						'level = level + 1
						'If level > 5 Then level = 1
						'world.Clear()
						'BuildLevel(level)
						'ResetPlayer(level)
							
						'hasWon = True
					Endif
					
				Elseif tutStep = 14
				
					If Millisecs() >= tutTime + tutWait
						tutStep = 15
						tutTime = Millisecs()
						tutWait = 4000
					Endif
					
				Elseif tutStep = 15
				
					If Millisecs() >= tutTime + tutWait
						tutStep = 16
						tutTime = Millisecs()
						tutWait = 2000
					Endif
					
				Elseif tutStep = 16
				
					#If TARGET = "glfw"
						If JoyHit(JOY_B)
							tutStep = 17
							tutTime = Millisecs()
							tutWait = 3000
						Endif
					#Endif
					
					'Check Menu Button
					'If selector = 9999
						If VTouchX() >= 1715.0 And VTouchX() <= 1885.0
							If VTouchY() >= 25.0 And VTouchY() <= 80.0
								If TouchHit()
									tutStep = 17
									tutTime = Millisecs()
									tutWait = 3000
								Endif
								selector = 2
							Else
								selector = 9999
							Endif
						Else
							selector = 9999
						Endif
					'Endif
					
				Elseif tutStep = 17
					
					If Millisecs() >= tutTime + tutWait
						tutStep = 18
						tutTime = Millisecs()
						tutWait = 2000
					Endif
					
				Elseif tutStep = 18
					'Print "HERE"
					If Millisecs() >= tutTime + tutWait
						tutorial = False
						tutStep = 0
						isPlaying = False
						stage = 1
						level = 1
						main_menu = True
						world.Clear()
					Endif
					
				Endif
				
				'Self.world.Update()	
			Endif
			
			Self.world.Update()
			
		Else
		
			If main_menu = True And options = False And credits = False
				
					#If TARGET = "glfw"
						If gamepad = True
							If selector = 1
								If JoyHit(JOY_RIGHT)
									selector = 3
								Elseif JoyHit(JOY_LEFT)
									selector = 2
								EndIf
							Elseif selector = 2
								If JoyHit(JOY_RIGHT)
									selector = 1
								Elseif JoyHit(JOY_LEFT)
									selector = 3
								EndIf
							Elseif selector = 3
								If JoyHit(JOY_RIGHT)
									selector = 2
								Elseif JoyHit(JOY_LEFT)
									selector = 1
								Endif
							Endif
							
							If JoyHit(JOY_A)
								If selector = 1
									main_menu = False
								Elseif selector = 2
									tutorial = True
									tutStep = 0
									isPlaying = True
									stage = 0
									level = 1
									tutTime = Millisecs()
									tutWait = 2000
									main_menu = False
									world.Clear()
									selector = 9999
								Elseif selector = 3
									options = True
									selector = 1
								EndIf
							Endif
						EndIf
						
					#End If
					
					If gamepad = False
					
						'Play Button
						'If selector = 9999
							If VTouchX() >= 700.0 And VTouchX() <= 1220.0
								If VTouchY() >= 635.0 And VTouchY() <= 875.0
									If TouchHit()
										main_menu = False
									Endif
									selector = 1
								Else
									selector = 9999
								Endif
							Else
								selector = 9999
							Endif
						'EndIf
						
						If selector = 9999
							'Tutorial Button
							If VTouchX() >= 144.0 And VTouchX() <= 575.0
								If VTouchY() >= 677.0 And VTouchY() <= 864.0
									If TouchHit()
										tutorial = True
										tutStep = 0
										isPlaying = True
										stage = 0
										level = 1
										tutTime = Millisecs()
										tutWait = 2000
										main_menu = False
										world.Clear()
										'LoadLevel(level)
										'player.Kill()
									Endif
									selector = 2
								Else
									selector = 9999
								Endif
							Else
								selector = 9999
							Endif
						EndIf
						
						If selector = 9999
							'Options Button
							If VTouchX() >= 1343.0 And VTouchX() <= 1774.0
								If VTouchY() >= 677.0 And VTouchY() <= 864.0
									If TouchHit()
										options = True
										selector = 1
									Endif
									selector = 3
								Else
									selector = 9999
								Endif
							Else
								selector = 9999
							Endif
						Endif
						
					EndIf
				
			Elseif levelComplete = True And credits = False
				
				'If TouchHit()
				
					#If TARGET = "glfw"
						If gamepad = True
							If selector = 1
								If JoyHit(JOY_RIGHT)
									selector = 2
								Elseif JoyHit(JOY_LEFT)
									selector = 2
								Elseif JoyHit(JOY_UP)
									selector = 3
								Elseif JoyHit(JOY_DOWN)
									selector = 3
								EndIf
							Elseif selector = 2
								If JoyHit(JOY_RIGHT)
									selector = 1
								Elseif JoyHit(JOY_LEFT)
									selector = 1
								Elseif JoyHit(JOY_UP)
									selector = 3
								Elseif JoyHit(JOY_DOWN)
									selector = 3
								EndIf
							Elseif selector = 3
								If JoyHit(JOY_RIGHT)
									selector = 2
								Elseif JoyHit(JOY_LEFT)
									selector = 1
								Elseif JoyHit(JOY_UP)
									selector = 1
								Elseif JoyHit(JOY_DOWN)
									selector = 1
								Endif
							
							Endif
							
							If JoyHit(JOY_A)
								If selector = 1
										level = level + 1
										If level = 9
											If stage = 1
												If gameProgress = 0
													gameProgress = 1
													gameProgressTimer = Millisecs()
													main_menu = False
													levelComplete = False
													selector = ((stage - 1)* 8) + level
													'stage = stage + 1
												Endif
											Elseif stage = 2
												If gameProgress = 2
													gameProgress = 3
													main_menu = False
													levelComplete = False
													gameProgressTimer = Millisecs()
													selector = ((stage - 1)* 8) + level
													'stage = stage + 1
												Endif
											Elseif stage = 3
												If gameProgress = 4
													'level = 24
													gameProgress = 5
													main_menu = False
													levelComplete = False
													gameProgressTimer = Millisecs()
													selector = ((stage - 1)* 8) + 24
													'Print "HERE"
												Endif
											Endif
											'If gameProgress <> 5
												'stage = 1
												'main_menu = True
											'Else
												If stage < 3
													stage = stage + 1
													level = 1
													selector = ((stage - 1)* 8) + level
												Else
													stage = 3
													level = 8
													selector = ((stage - 1)* 8) + level
													isPlaying = False
													'main_menu = True
												EndIf
												'Print gameProgress
											'EndIf
										Endif
										If gameProgress <> 1 And gameProgress <> 3 And gameProgress <> 5
											world.Clear()
											BuildLevel(level)
											ResetPlayer(level)
											levelComplete = False
											isPlaying = True
											selector = 0
										EndIf
									
								Elseif selector = 2
									main_menu = False
									levelComplete = False
									selector = ((stage - 1)* 8) + level
								Elseif selector = 3
									Local starText:String = "Stars"
									If tStars = 1 Then starText = "Star"
									Local levelNumber:Int
									If stage = 1
										levelNumber = level
									Else
										levelNumber = level + (8*(stage - 1))
									EndIf
									DoTweet.LaunchTwitter("", "I completed Level " + levelNumber + " and earned " + tStars + " " + starText + "!", "ReboundGame")
								EndIf
							Endif
							
							If JoyHit(JOY_X)
								isPlaying = True
								levelComplete = False
								ResetPlayer(level)
								selector = 1
							Endif
							
							If JoyHit(JOY_B)
								main_menu = False
								levelComplete = False
								selector = ((stage - 1)* 8) + level
							Endif
						EndIf
						
						
					#End If
					
					If gamepad = False
				
						If VTouchY() >= 700.0 And VTouchY() <= 803.0
						
							'Check Menu Button
							If VTouchX() >= 702.0 And VTouchX() <= 939.0
								If TouchHit()
									main_menu = False
									levelComplete = False
								Endif
								selector = 2
							Else
								selector = 9999
							Endif
							
							'Check Next Button
							If selector = 9999
								If VTouchX() >= 981.0 And VTouchX() <= 1254.0
									If TouchHit()
										level = level + 1
										If level = 9
											If stage = 1
												If gameProgress = 0
													gameProgress = 1
													gameProgressTimer = Millisecs()
													main_menu = False
													levelComplete = False
													selector = ((stage - 1)* 8) + level
													'stage = stage + 1
												Endif
											Elseif stage = 2
												If gameProgress = 2
													gameProgress = 3
													main_menu = False
													levelComplete = False
													gameProgressTimer = Millisecs()
													selector = ((stage - 1)* 8) + level
													'stage = stage + 1
												Endif
											Elseif stage = 3
												If gameProgress = 4
													'level = 24
													gameProgress = 5
													main_menu = False
													levelComplete = False
													gameProgressTimer = Millisecs()
													selector = ((stage - 1)* 8) + 24
												Endif
											Endif
											If stage < 3
												stage = stage + 1
												level = 1
												selector = ((stage - 1)* 8) + level
											Else
												stage = 3
												level = 8
												selector = ((stage - 1)* 8) + level
												isPlaying = False
												'main_menu = True
											EndIf
										Endif
										If gameProgress <> 1 And gameProgress <> 3 And gameProgress <> 5
											LoadLevel(level)
											touchDelayStart = Millisecs() + 150
											'world.Clear()
											'BuildLevel(level)
											'ResetPlayer(level)
											'levelComplete = False
											'isPlaying = True
										Endif
										touchDelayStart = Millisecs() + 150
									Endif
									selector = 1
								Else
									selector = 9999
								Endif
							Endif
							
						Else
						
							selector = 9999
							
						Endif
						
						'Redo
						If selector = 9999
							If VTouchX() >= 25.0 And VTouchX() <= 200.0
								If VTouchY() >= 25.0 And VTouchY() <= 200.0
									If TouchHit()
										isPlaying = True
										levelComplete = False
										ResetPlayer(level)
									Endif
									selector = 4
								Else
									selector = 9999
								Endif
							Else
								selector = 9999
							Endif
						EndIf
						
						'Redo Key Pressed
						If KeyHit(KEY_R) Or JoyHit(JOY_X)
							isPlaying = True
							levelComplete = False
							ResetPlayer(level)
						Endif
						
						
						'Check Menu Button
						If selector = 9999
							If VTouchX() >= 1715.0 And VTouchX() <= 1885.0
								If VTouchY() >= 25.0 And VTouchY() <= 80.0
									If TouchHit()
										levelComplete = False
										isPlaying = False
									Endif
									selector = 5
								Else
									selector = 9999
								Endif
							Else
								selector = 9999
							Endif
						Endif
						
						If selector = 9999
							If VTouchX() >= 922.0 And VTouchX() <= 997.0
								If VTouchY() >= 291.0 And VTouchY() <= 351.0
									If TouchHit()
										Local starText:String = "Stars"
										If tStars = 1 Then starText = "Star"
										Local levelNumber:Int
										If stage = 1
											levelNumber = level
										Else
											levelNumber = level + (8*(stage - 1))
										EndIf
										DoTweet.LaunchTwitter("", "I completed Level " + levelNumber + " and earned " + tStars + " " + starText + "!", "ReboundGame")
										Print "HERE"
									Endif
									selector = 3
								Else
									selector = 9999
								Endif
							Else
								selector = 9999
							Endif
						EndIf
					
				Endif
				
			Elseif options = True And credits = False
			
				#If TARGET = "glfw"
					If gamepad = True
						
						If JoyHit(JOY_RIGHT)
							If selector = 4
								selector = 5
							Elseif selector = 6
								selector = 7
							Endif
						Elseif JoyHit(JOY_LEFT)
							If selector = 5
								selector = 4
							Elseif selector = 7
								selector = 6
							Endif
						Elseif JoyHit(JOY_DOWN)
							If selector = 1
								selector = 2
							Elseif selector = 2
								selector = 3
							Elseif selector = 3
								selector = 4
							Elseif selector = 4
								selector = 6
							Elseif selector = 6
								selector = 8
							Elseif selector = 5
								selector = 7
							Elseif selector = 7
								selector = 8
							Elseif selector = 8
								selector = 1
							Endif
						Elseif JoyHit(JOY_UP)
							If selector = 1
								selector = 8
							Elseif selector = 2
								selector = 1
							Elseif selector = 3
								selector = 2
							Elseif selector = 4
								selector = 3
							Elseif selector = 6
								selector = 4
							Elseif selector = 5
								selector = 3
							Elseif selector = 7
								selector = 5
							Elseif selector = 8
								selector = 7
							Endif
						Elseif JoyHit(JOY_A)
							If selector = 1
								resX = 1024
								resY = 576
								SetResolution(1024, 576, fullscreen)
							Elseif selector = 2
								resX = 1280
								resY = 720
								SetResolution(1280, 720, fullscreen)
							Elseif selector = 3
								resX = 1920
								resY = 1080
								SetResolution(1920, 1080, fullscreen)
							Elseif selector = 4
								fullscreen = False
								If resX = 1024 And resY = 576
									SetResolution(1024, 576, fullscreen)
								Elseif resX = 1280 And resY = 720
									SetResolution(1280, 720, fullscreen)
								Elseif resX = 1920 And resY = 1080
									SetResolution(1920, 1080, fullscreen)
								EndIf
							Elseif selector = 5
								fullscreen = True
								If resX = 1024 And resY = 576
									SetResolution(1024, 576, fullscreen)
								Elseif resX = 1280 And resY = 720
									SetResolution(1280, 720, fullscreen)
								Elseif resX = 1920 And resY = 1080
									SetResolution(1920, 1080, fullscreen)
								EndIf
							Elseif selector = 6
								credits = True
								img_level.Discard()
								img_level = LoadImage("Graphics/UI/Credits1.png")
								creditStep = 1
								creditStart = Millisecs()
							Elseif selector = 7
								Error("")
							Elseif selector = 8
								options = False
								If main_menu = True
									selector = 3
								Else
									selector =12
								Endif
							Endif
						Elseif JoyHit(JOY_B)
							options = False
							If main_menu = True
								selector = 3
							Else
								selector =12
							EndIf
						EndIf
						
					EndIf
				#Endif
				
				If gamepad = False
					
					#If TARGET <> "android"
						'Res1
						If VTouchX() >= 849.0 And VTouchX() <= 1079.0
							If VTouchY() >= 233.0 And VTouchY() <= 313.0
								If TouchHit()
									resX = 1024
									resY = 576
									SetResolution(1024, 576, fullscreen)
								Endif
								selector = 1
							Else
								selector = 9999
							Endif
						Else
							selector = 9999
						Endif
						
						'Res2
						If selector = 9999
							If VTouchX() >= 849.0 And VTouchX() <= 1079.0
								If VTouchY() >= 329.0 And VTouchY() <= 412.0
									If TouchHit()
										resX = 1280
										resY = 720
										SetResolution(1280, 720, fullscreen)
									Endif
									selector = 2
								Else
									selector = 9999
								Endif
							Else
								selector = 9999
							Endif
						EndIf
						
						'Res3
						If selector = 9999
							If VTouchX() >= 849.0 And VTouchX() <= 1079.0
								If VTouchY() >= 425.0 And VTouchY() <= 506.0
									If TouchHit()
										resX = 1920
										resY = 1080
										SetResolution(1920, 1080, fullscreen)
									Endif
									selector = 3
								Else
									selector = 9999
								Endif
							Else
								selector = 9999
							Endif
						EndIf
						
						'Windowed
						If selector = 9999
							If VTouchX() >= 696.0 And VTouchX() <= 927.0
								If VTouchY() >= 517.0 And VTouchY() <= 600.0
									If TouchHit()
										fullscreen = False
										If resX = 1024 And resY = 576
											SetResolution(1024, 576, fullscreen)
										Elseif resX = 1280 And resY = 720
											SetResolution(1280, 720, fullscreen)
										Elseif resX = 1920 And resY = 1080
											SetResolution(1920, 1080, fullscreen)
										EndIf
									Endif
									selector = 4
								Else
									selector = 9999
								Endif
							Else
								selector = 9999
							Endif
						EndIf
						
						'Fullscreen
						If selector = 9999
							If VTouchX() >= 988.0 And VTouchX() <= 1221.0
								If VTouchY() >= 517.0 And VTouchY() <= 600.0
									If TouchHit()
										fullscreen = True
										If resX = 1024 And resY = 576
											SetResolution(1024, 576, fullscreen)
										Elseif resX = 1280 And resY = 720
											SetResolution(1280, 720, fullscreen)
										Elseif resX = 1920 And resY = 1080
											SetResolution(1920, 1080, fullscreen)
										EndIf
									Endif
									selector = 5
								Else
									selector = 9999
								Endif
							Else
								selector = 9999
							Endif
						EndIf
						
						'Credits
						If selector = 9999
							If VTouchX() >= 696.0 And VTouchX() <= 927.0
								If VTouchY() >= 629.0 And VTouchY() <= 713.0
									If TouchHit()
										credits = True
										img_level.Discard()
										img_level = LoadImage("Graphics/UI/Credits1.png")
										creditStep = 1
										creditStart = Millisecs()
									Endif
									selector = 6
								Else
									selector = 9999
								Endif
							Else
								selector = 9999
							Endif
						EndIf
						
						'Exit Game
						If selector = 9999
							If VTouchX() >= 988.0 And VTouchX() <= 1221.0
								If VTouchY() >= 629.0 And VTouchY() <= 713.0
									If TouchHit()
										Error("")
									Endif
									selector = 7
								Else
									selector = 9999
								Endif
							Else
								selector = 9999
							Endif
						EndIf
						
						'Return
						If selector = 9999
							If VTouchX() >= 849.0 And VTouchX() <= 1079.0
								If VTouchY() >= 747.0 And VTouchY() <= 828.0
									If TouchHit()
										options = False
										If main_menu = True
											selector = 3
										Else
											selector =12
										EndIf
									Endif
									selector = 8
								Else
									selector = 9999
								Endif
							Else
								selector = 9999
							Endif
						Endif
						
					#Else
						
						'Credits
						If VTouchX() >= 695.0 And VTouchX() <= 930.0
							If VTouchY() >= 418.0 And VTouchY() <= 500.0
								If TouchHit()
									credits = True
									img_level.Discard()
									img_level = LoadImage("Graphics/UI/Credits1.png")
									creditStep = 1
									creditStart = Millisecs()
								Endif
								selector = 6
							Else
								selector = 9999
							Endif
						Else
							selector = 9999
						Endif
						
						'Music
						If VTouchX() >= 988.0 And VTouchX() <= 1221.0
							If VTouchY() >= 418.0 And VTouchY() <= 500.0
								If TouchHit()
									If musicPlaying = True
										PauseMusic()
										musicPlaying = False
										SaveState(gameData.SaveString(gameProgress,0))
									Elseif musicPlaying = False
										ResumeMusic()
										musicPlaying = True
										SaveState(gameData.SaveString(gameProgress, 1))
									EndIf
								Endif
								selector = 6
							Else
								selector = 9999
							Endif
						Else
							selector = 9999
						Endif
						
						'Return
						If VTouchX() >= 850.0 And VTouchX() <= 1084.0
							If VTouchY() >= 534.0 And VTouchY() <= 615.0
								If TouchHit()
									options = False
									If main_menu = True
										selector = 3
									Else
										selector =12
									EndIf
								Endif
								selector = 8
							Else
								selector = 9999
							Endif
						Else
							selector = 9999
						Endif
						
					#EndIf
				
				EndIf
			
			Elseif credits = True
			
				If Millisecs() > creditStart + 4000
					creditStep = creditStep + 1
					img_level.Discard()
					If creditStep = 2
						img_level = LoadImage("Graphics/UI/Credits2.png")
					ElseIf creditStep = 3
						img_level = LoadImage("Graphics/UI/Credits3.png")
					ElseIf creditStep = 4
						img_level = LoadImage("Graphics/UI/Credits4.png")
					ElseIf creditStep = 5
						img_level = LoadImage("Graphics/UI/Credits5.png")
					ElseIf creditStep = 6
						img_level = LoadImage("Graphics/UI/Credits6.png")
					EndIf
					If creditStep >= 7
						creditStep = 1
						credits = False
					Endif
					creditStart = Millisecs()
				Endif
				
			Else
			
				'If TouchHit()
				
					#If TARGET = "glfw"
						If gamepad = True
							If JoyHit(JOY_RIGHT)
								If selector < 4
									selector = selector + 1
								Elseif selector = 4
									If stage < 3
										selector = 10
									Else
										selector = 5
									ENdIf
								Elseif selector >= 5 And selector < 8
									selector = selector + 1
								Elseif selector = 8
									If stage < 3
										selector = 10
									EndIf
								Elseif selector = 9
									selector = 1
								Elseif selector = 11
									selector = 12
								Elseif selector = 12
									selector = 11
								Endif
							Elseif JoyHit(JOY_LEFT)
								If selector <= 4
									selector = selector - 1
									If selector < 1 
										If stage > 1
											selector = 9
										Else
											selector = 1
										Endif
									ENdIf
								Elseif selector >= 5 And selector <= 8
									selector = selector - 1
									If selector = 4 
										If stage > 1
											selector = 9
										Endif
									ENdIf
								Elseif selector = 10
									selector = 8
								Elseif selector = 12
									selector = 11
								Elseif selector = 11
									selector = 12
								Endif
								
							Elseif JoyHit(JOY_UP)
							
								If selector >= 5 And selector < 9
								
									If selector = 5
										selector = 1
									Elseif selector = 6
										selector = 2
									Elseif selector = 7
										selector = 3
									Elseif selector = 8
										selector = 4
									Endif
									
								Elseif selector = 11
									selector = 5
								Elseif selector = 12
									selector = 8
								Endif
								
							Elseif JoyHit(JOY_DOWN)
								
								If selector >= 1 And selector < 5
								
									If selector = 1
										selector = 5
									Elseif selector = 2
										selector = 6
									Elseif selector = 3
										selector = 7
									Elseif selector = 4
										selector = 8
									Endif
									
								Elseif selector = 5 Or selector = 6
									selector = 11
									
								Elseif selector = 7 Or selector = 8
									selector = 12
									
								Endif
									
								
							Endif
							
							If JoyHit(JOY_A)
								If selector >= 1 And selector <= 8
									level = selector
									If gameData.stage[stage - 1].level[level - 1].unlocked = True
										LoadLevel(level)
										selector = 0
									Endif
								Elseif selector = 9
									stage = stage - 1
									If stage < 2 
										stage = 1
										selector = 1
									EndIf
								Elseif selector = 10
									stage = stage + 1
									If stage > 3 
										stage = 3
										selector = 1
									EndIf
								Elseif selector = 11
									main_menu = True
									selector = 1
								Elseif selector = 12
									options = True
									selector = 1
								Endif
							EndIf
							
							If JoyHit(JOY_B)
								main_menu = True
								selector = 1
							EndIf
						EndIf
							
						
					#Endif
					
					If gamepad = False
						
						'Check top row
						If VTouchY() >= 84.0 And VTouchY() <= 386.0
						
							'1
							If VTouchX() >= 216.0 And VTouchX() <= 430.0
								If TouchHit()
									level = 1
									If gameData.stage[stage - 1].level[level - 1].unlocked = True
										LoadLevel(level)
										touchDelayStart = Millisecs() + 150
									Endif
								Endif
								selector = 1
								
							'2
							Elseif VTouchX() >= 648.0 And VTouchX() <= 862.0
								If TouchHit()
									level = 2
									If gameData.stage[stage - 1].level[level - 1].unlocked = True
										LoadLevel(level)
										touchDelayStart = Millisecs() + 150
									Endif
								Endif
								selector = 2
							
							'3
							Elseif VTouchX() >= 1078.0 And VTouchX() <= 1292.0
								If TouchHit()
									level = 3
									'level = (((stage - 1) * 8) + level)
									If gameData.stage[stage - 1].level[level - 1].unlocked = True
										LoadLevel(level)
										touchDelayStart = Millisecs() + 150
									Endif
								Endif
								selector = 3
								
							'4
							Elseif VTouchX() >= 1512.0 And VTouchX() <= 1726.0
								If TouchHit()
									level = 4
									If gameData.stage[stage - 1].level[level - 1].unlocked = True
										LoadLevel(level)
										touchDelayStart = Millisecs() + 150
									Endif
								Endif
								selector = 4
								
							Else
							
								selector = 9999
								
							Endif
						
						'Check bottom row	
						Elseif VTouchY() >= 573.0 And VTouchY() <= 875.0
							
							'5
							If VTouchX() >= 216.0 And VTouchX() <= 430.0
								If TouchHit()
									level = 5
									If gameData.stage[stage - 1].level[level - 1].unlocked = True
										LoadLevel(level)
										touchDelayStart = Millisecs() + 150
									Endif
								Endif
								selector = 5
								
							'6
							Elseif VTouchX() >= 648.0 And VTouchX() <= 862.0
								If TouchHit()
									level = 6
									If gameData.stage[stage - 1].level[level - 1].unlocked = True
										LoadLevel(level)
										touchDelayStart = Millisecs() + 150
									Endif
								Endif
								selector = 6
							
							'7
							Elseif VTouchX() >= 1078.0 And VTouchX() <= 1292.0
								If TouchHit()
									level = 7
									If gameData.stage[stage - 1].level[level - 1].unlocked = True
										LoadLevel(level)
										touchDelayStart = Millisecs() + 150
									Endif
								Endif
								selector = 7
								
							'8
							Elseif VTouchX() >= 1512.0 And VTouchX() <= 1726.0
								If TouchHit()
									level = 8
									If gameData.stage[stage - 1].level[level - 1].unlocked = True
										LoadLevel(level)
										touchDelayStart = Millisecs() + 150
									Endif
								Endif
								selector = 8
								
							Else
							
								selector = 9999
								
							Endif
							
						Else
							
							selector = 9999
							
						Endif
						
						If selector = 9999
						
							'Right Arrow
							If VTouchX() >= 1808 And VTouchX() <= 1885
								If VTouchY() >= 413 And VTouchY() <= 530
									If TouchHit()
										stage = stage + 1
										If stage > 3 Then stage = 3
									Endif
									selector = 10
								Else
									selector = 9999
								Endif
							Endif
							
							'Left Arrow
							If VTouchX() >= 38 And VTouchX() <= 114
								If VTouchY() >= 416 And VTouchY() <= 542
									If TouchHit()
										stage = stage - 1
										If stage < 1 Then stage = 1
									Endif
									selector = 9
								Else
									selector = 9999
								Endif
							Endif
							
							If selector = 9999
								'Menu Button
								If VTouchX() >= 0.0 And VTouchX() <= 380.0
									If VTouchY() >= 940.0 And VTouchY() <= 1080.0
										If TouchHit()
											main_menu = True
										EndIf
										selector = 11
									Else
										selector = 9999
									Endif
								Endif
								
								'Options Button
								If VTouchX() >= 1537.0 And VTouchX() <= 1920.0
									If VTouchY() >= 940.0 And VTouchY() <= 1080.0
										If TouchHit()
											options = True
											selector = 1
										EndIf
										selector = 12
									Else
										selector = 9999
									Endif
								Endif
							EndIf
							
						Endif
						
				
						
				Endif
			
			Endif
			
		Endif
		
		#If TARGET = "glfw"
			If gamepad = False
				If JoyHit(JOY_RIGHT) Or JoyHit(JOY_LEFT) Or JoyHit(JOY_UP) Or JoyHit(JOY_DOWN) Or JoyHit(JOY_A) Or JoyHit(JOY_B) Or JoyHit(JOY_X) Or JoyHit(JOY_Y) Or JoyHit(JOY_START) Or JoyHit(JOY_BACK) Or JoyZ() < -0.01 Or JoyZ() > 0.01
					selector = 1
					gamepad = True
					mX = VTouchX()
					mY = VTouchY()
					HideMouse()
					'Print "GAMEPAD ENABLED"
				ENdIf
			Else
				If VTouchX() > mX + 5 Or VTouchX() < mX - 5
					gamepad = False
					selector = 9999
					ShowMouse()
					'Print "GAMEPAD DISABLED"
				Endif
				If VTouchY() > mY + 5 Or VTouchX() < mY - 5
					gamepad = False
					selector = 9999
					ShowMouse()
					'Print "GAMEPAD DISABLED"
				Endif
			Endif
		#End If
		
		
		
		'Exit Game
		#If TARGET <> "android"
			If KeyHit(KEY_ESCAPE) Then Error("")
		#Endif
			
		'UpdateAsyncEvents()
	
		'Return 0
		Return 0
		
	End Method
	
	Method LoadLevel:Void(l:Int)
			
		world.Clear()
		world = New Box2D_World(0.0, 10.0, 64.0, debug)
		LoadLevelBackground(l)
		BuildLevel(l)
		ResetPlayer(l)
		isPlaying = True
		levelStartTimer = Millisecs()
		selector = 9999
		touchDelayStart = Millisecs()
			
	End Method
	
	Method LoadLevel2:Void(l:Int)
			
		world.Clear()
		world = New Box2D_World(0.0, 10.0, 64.0, debug)
		BuildLevel(l)
		ResetPlayer(l)
		isPlaying = True
		levelStartTimer = Millisecs()
		selector = 9999
		touchDelayStart = Millisecs()
			
	End Method
	
	Method BuildLevel:Void(l:Int)
		world.Clear()
		If stage = 0
			If l = 1
				world.world.SetGravity(New b2Vec2(0.0, 1.0))
				CreateLevel1()
			Elseif l = 2
				world.world.SetGravity(New b2Vec2(0.0, 10.0))
				CreateLevel1()
			EndIf
		ElseIf stage = 1
			world.world.SetGravity(New b2Vec2(0.0, 10.0))
			If l = 1
				CreateLevel1()
			Elseif l = 2
				CreateLevel2()
			Elseif l = 3
				CreateLevel3()
			Elseif l = 4
				CreateLevel4()
			Elseif l = 5
				CreateLevel5()
			Elseif l = 6
				CreateLevel6()
			Elseif l = 7
				CreateLevel7()
			Elseif l = 8
				CreateLevel8()
			Endif
		Elseif stage = 2
			world.world.SetGravity(New b2Vec2(0.0, 0.0))
			If l = 1
				CreateLevel9()
			Elseif l = 2
				CreateLevel10()
			Elseif l = 3
				CreateLevel11()
			Elseif l = 4
				CreateLevel1()
			Elseif l = 5
				CreateLevel13()
			Elseif l = 6
				'CreateLevel6()
			Elseif l = 7
				CreateLevel1()
			Elseif l = 8
				CreateLevel1()
			Endif
		Elseif stage = 3
			world.world.SetGravity(New b2Vec2(0.0, 10.0))
			If l = 1
				CreateLevel17()
			Elseif l = 2
				CreateLevel18()
			Elseif l = 3
				CreateLevel19()
			Elseif l = 4
				CreateLevel20()
			Elseif l = 5
				CreateLevel8()
			Elseif l = 6
				CreateLevel22()
			Elseif l = 7
				CreateLevel23()
			Elseif l = 8
				CreateLevel1()
			Endif
		ENdIf
	End Method
	
	Method CreateExit:Void(l:Int)
		If stage = 0
			If l = 1
				CreateExit(1700, 957)
			Elseif l = 2
				CreateExit(700, 957)
			EndIf
		ElseIf stage = 1
			If l = 1
				CreateExit(1700, 957)
			Elseif l = 2
				CreateExit(1700, 957)
			Elseif l = 3
				CreateExit(1700, 840)
			Elseif l = 4
				CreateExit(1700, 957)
			Elseif l = 5
				CreateExit(1700, 600)
			Elseif l = 6
				CreateExit(910, 800)
			Elseif l = 7
				CreateExit(1700, 957)
			Elseif l = 8
				CreateExit(1700, 840)
			Endif
		ElseIf stage = 2
			If l = 1
				CreateExit(1575, 957)
			Elseif l = 2
				CreateExit(1700, 515)
			Elseif l = 3
				CreateExit(715, 535)
			Elseif l = 4
				CreateExit(535, 957)
			Elseif l = 5
				CreateExit(910, 535)
			Elseif l = 6
				CreateExit(910, 900)
			Elseif l = 7
				CreateExit(910, 515)
			Elseif l = 8
				CreateExit(1700, 515)
			Endif
		ElseIf stage = 3
			If l = 1
				CreateExit(1700, 840)
			Elseif l = 2
				CreateExit(1700, 957)
			Elseif l = 3
				CreateExit(480, 800)
			Elseif l = 4
				CreateExit(1494, 988)
			Elseif l = 5
				CreateExit(1700, 840)
			Elseif l = 6
				CreateExit(1565, 669)
			Elseif l = 7
				CreateExit(1700, 777)
			Elseif l = 8
				CreateExit(1700, 957)
			Endif
		EndIf
	End Method
	
	'Runs when app is ready to render
	Method OnRender:Int()
		
		'Update the Virtual Resolution
		UpdateVirtualDisplay()
		
		'Clear Screen
		Cls(255,255,255)
		
		'Is currently playing the game
		If isPlaying = True
		
			If tutorial = False
				'Draw Level
				DrawLevel(level)
				
				'Draw Time
				Local st:String = String(((Millisecs() - levelStartTimer)/1000)) +  "."
				Local mm:String = String(Millisecs() - levelStartTimer)
				Local len:Int = mm.Length()
				st = st + mm[(len - 3)..(len - 2)]
				fnt_54Font.DrawText(st, 960, 40, eDrawAlign.CENTER)
				
				'Render Physics World
				Self.world.Render()
				
			Elseif tutorial = True
				
				If tutStep = 0
					fnt_54Font.DrawText("WELCOME TO REBOUND", 960, 540, eDrawAlign.CENTER)
					'fnt_54Font.DrawText("THE CHALLENGING PHYSICS PLATFORMER GAME", 960, 550, eDrawAlign.CENTER)
				Elseif tutStep = 1
					fnt_54Font.DrawText("LETS GET STARTED", 960, 540, eDrawAlign.CENTER)
				Elseif tutStep = 2
					fnt_54Font.DrawText("THIS IS YOU", 960, 440, eDrawAlign.CENTER)
					DrawImage(img_player, 960, 640)
				Elseif tutStep = 3
					fnt_54Font.DrawText("YOU LIKE TO SPIN", 960, 440, eDrawAlign.CENTER)
					DrawImage(img_player, 960, 640)
				Elseif tutStep = 4
					#If TARGET <> "android"
						fnt_54Font.DrawText("HOLD THE D KEY OR RIGHT ARROW KEY TO SPIN RIGHT", 960, 440, eDrawAlign.CENTER)
					#Else
						fnt_54Font.DrawText("TOUCH AND HOLD THE RIGHT SIDE OF THE SCREEN", 960, 380, eDrawAlign.CENTER)
						fnt_54Font.DrawText("TO SPIN RIGHT", 960, 440, eDrawAlign.CENTER)
					#EndIf
					'DrawImage(img_player, 960, 640)
				Elseif tutStep = 5
					fnt_54Font.DrawText("NICE WORK", 960, 440, eDrawAlign.CENTER)
					'DrawImage(img_player, 960, 640)
				Elseif tutStep = 6
					#If TARGET <> "android"
						fnt_54Font.DrawText("HOLD THE A KEY OR LEFT ARROW KEY TO SPIN LEFT", 960, 440, eDrawAlign.CENTER)
					#Else
						fnt_54Font.DrawText("TOUCH AND HOLD THE LEFT SIDE OF THE SCREEN", 960, 380, eDrawAlign.CENTER)
						fnt_54Font.DrawText("TO SPIN LEFT", 960, 440, eDrawAlign.CENTER)
					#EndIf
				Elseif tutStep = 7
					fnt_54Font.DrawText("GREAT", 960, 440, eDrawAlign.CENTER)
				Elseif tutStep = 8
					fnt_54Font.DrawText("THIS IS HOW THE TYPICAL GAME SCREEN LOOKS", 960, 380, eDrawAlign.CENTER)
				Elseif tutStep = 9
					fnt_54Font.DrawText("THE GOAL IS TO REACH THE EXIT", 960, 380, eDrawAlign.CENTER)
				Elseif tutStep = 10
					fnt_54Font.DrawText("NOW LETS ADD SOME GRAVITY", 960, 380, eDrawAlign.CENTER)
				Elseif tutStep = 11
					fnt_54Font.DrawText("TRY TO TILT YOURSELF TO REBOUND TOWARDS THE EXIT", 960, 380, eDrawAlign.CENTER)
				Elseif tutStep = 12
					#If TARGET <> "android"
						fnt_54Font.DrawText("CLICK THE REDO BUTTON OR PRESS THE R KEY TO", 960, 380, eDrawAlign.CENTER)
						fnt_54Font.DrawText("RESTART THE LEVEL", 960, 440, eDrawAlign.CENTER)
					#Else
						fnt_54Font.DrawText("TOUCH THE REDO BUTTON TO RESTART THE LEVEL", 960, 380, eDrawAlign.CENTER)
					#Endif
				Elseif tutStep = 13
					fnt_54Font.DrawText("NOW REACH THE EXIT", 960, 380, eDrawAlign.CENTER)
				Elseif tutStep = 14
					fnt_54Font.DrawText("GREAT JOB", 960, 380, eDrawAlign.CENTER)
				Elseif tutStep = 15
					fnt_54Font.DrawText("THE FASTER YOU REACH THE EXIT THE MORE STARS", 960, 380, eDrawAlign.CENTER)
					fnt_54Font.DrawText("YOU WILL EARN", 960, 440, eDrawAlign.CENTER)
				Elseif tutStep = 16
					#If TARGET <> "android"
						fnt_54Font.DrawText("CLICK THE MENU BUTTON AT ANY TIME TO RETURN", 960, 380, eDrawAlign.CENTER)
						fnt_54Font.DrawText("TO THE MENU", 960, 440, eDrawAlign.CENTER)
					#Else
						fnt_54Font.DrawText("TOUCH THE MENU BUTTON AT ANY TIME TO RETURN", 960, 380, eDrawAlign.CENTER)
						fnt_54Font.DrawText("TO THE MENU", 960, 440, eDrawAlign.CENTER)
					#Endif
				Elseif tutStep = 17
					fnt_54Font.DrawText("THIS CONCLUDES THE TUTORIAL", 960, 540, eDrawAlign.CENTER)
				Elseif tutStep = 18
					fnt_54Font.DrawText("GOOD LUCK", 960, 540, eDrawAlign.CENTER)
				Endif
				If tutStep >= 8 And tutStep < 17
					DrawImage(img_level, 0, 0)
					
					#If TARGET <> "android"
						'Draw Redo Button
						'If levelComplete = False
							If selector = 1
								DrawImage(img_redoH, 25, 25)
							Else
								DrawImage(img_redo, 25, 25)
							Endif
						'Else
						'	DrawImage(img_redo, 25, 25)
						'EndIf
					#Else
						DrawImage(img_redo, 25, 25)
					#ENdIf
						
					'Draw Exit
					DrawImage(img_exit, exitX, exitY)
			
					
					#If TARGET <> "android" 
						'Draw Menu
						'If levelComplete = False
							If selector = 2
								DrawImage(img_menuH2, 1715, 25)
							Else
								DrawImage(img_menu, 1715, 25)
							Endif
						'Else
						'	DrawImage(img_menu, 1715, 25)
						'Endif
					#Else
						DrawImage(img_menu, 1715, 25)
					#Endif
					
				Endif
				Self.world.Render()
			Endif
			
		'Is in the menus
		Else
		
			If main_menu = True And options = False And credits = False
				DrawImage(img_mainMenu, 0, 0)
				
				#If TARGET <> "android"
					If selector = 1
						DrawImage(img_playH, 708.0, 647.0)
					ElseIf selector = 2
						DrawImage(img_tutorialH, 144.0, 679.0)
					ElseIf selector = 3
						DrawImage(img_optionsH2, 1343.0, 677.0)
					Endif
				#EndIf
				
			Elseif levelComplete = True And credits = False
				
				'Draw Level
				DrawLevel(level)
				
				'Draw Time
				'fnt_stageSelectFont.DrawText(st, 960, 80, eDrawAlign.CENTER)
				
				'Draw Level Complete
				DrawImage(img_levelComplete, 960, 470)
				
				#If TARGET <> "android"
					If selector = 1
						DrawImage(img_nextH, 981.0, 700)
					Elseif selector = 2
						DrawImage(img_menuH3, 702.0, 700)
					Elseif selector = 3
						DrawImage(img_twitterH, 922.0, 291)
					Elseif selector = 4
						DrawImage(img_redoH, 25, 25)
					Elseif selector = 5
						DrawImage(img_menuH2, 1715, 25)
					Endif
				#Endif
				
				'Draw Level -- complete
				fnt_72Font.DrawText("LEVEL " + (level + ((stage - 1) * 8)), 960, 110, eDrawAlign.CENTER)
				
				'Draw current time
				fnt_timeFont.DrawText(fTime, 1005, 378)
				
				'Draw best time
				Local tt:Int = gameData.stage[stage - 1].level[level - 1].bestTime
				Local st:String = String((tt)/1000) +  "."
				Local mm:String = String(tt)
				Local len:Int = mm.Length()
				st = st + mm[(len - 3)..(len - 2)]
				fnt_timeFont.DrawText(st , 1005, 453)
				
				'Draw stars earned
				If tStars = 1
					DrawImage(img_starFullLarge, 800, 595)
				Elseif tStars = 2
					DrawImage(img_starFullLarge, 800, 595)
					DrawImage(img_starFullLarge, 915, 595)
				Elseif tStars = 3
					DrawImage(img_starFullLarge, 800, 595)
					DrawImage(img_starFullLarge, 915, 595)
					DrawImage(img_starFullLarge, 1033, 595)
				EndIf
			
			Elseif options = True And credits = False
			
				If main_menu = True
					DrawImage(img_mainMenu, 0, 0)
				Else
					DrawImage(img_stageSelect, 0, 0)
					
					If stage = 1
					
						'Remove Left Arrow
						DrawRect(30, 400, 100, 165)
						
					Elseif stage = 3
						
						'Remove Right Arrow
						DrawRect(1790, 400, 100, 165)
						
					Endif
				
					'Draw Numbers on unlocked levels
					Local x:Float = 325.0
					Local y:Float = 125.0
					Local sx:Float = 243.0
					Local sy:Float = 315.0
					Local add:Int = (stage - 1) * 8
					For Local i:Int = 0 To 7
						If gameData.stage[stage - 1].level[i].unlocked = True
							DrawRect(x - 50, y + 5, 90, 135)
							'If selector <> (i + 1)
								fnt_stageSelectFont.DrawText(gameData.stage[stage - 1].level[i].ID + 1 + add, x, y, eDrawAlign.CENTER)

							'EndIf
						Endif
						Local stars:Int = gameData.stage[stage - 1].level[i].starsEarned
						'If selector <> (i + 1)
							If stars = 1
								DrawImage(img_starFull, sx, sy)
							Elseif stars = 2
								DrawImage(img_starFull, sx, sy)
								DrawImage(img_starFull, sx + 58, sy)
							Elseif stars = 3
								DrawImage(img_starFull, sx, sy)
								DrawImage(img_starFull, sx + 58, sy)
								DrawImage(img_starFull, sx + 116, sy)
							Endif
						'EndIf
						sx = sx + 432.0
						If sx > 1540.0
							sx = 243.0
							sy = 804.0
						Endif
							
						x = x + 432.0
						If x > 1621
							x = 325
							y = 614
						Endif
					Next
				Endif

				#If TARGET <> "android"
					DrawImage(img_options, 960, 470)
				#Else
					DrawImage(img_androidOptions, 960, 470)
					If musicPlaying = True
						DrawImage(img_on, 1140, 449)
					Elseif musicPlaying = False
						DrawImage(img_off, 1138, 449)
					EndIf 
				#EndIf
				
				#If TARGET <> "android"
					'X: 660
					'Y: 140
					If selector = 1
						DrawImage(img_res1H, 899, 265)
					Elseif selector = 2
						DrawImage(img_res2H, 899, 297 + 65)
					Elseif selector = 3
						DrawImage(img_res3H, 890, 387 + 70)
					Elseif selector = 4
						DrawImage(img_windowedH, 731, 481 + 70)
					Elseif selector = 5
						DrawImage(img_fullscreenH, 1017, 481 + 70)
					Elseif selector = 6
						DrawImage(img_creditsH, 739, 592 + 70)
					Elseif selector = 7
						DrawImage(img_exitGameH, 1013, 592 + 70)
					Elseif selector = 8
						DrawImage(img_returnH, 894, 708 + 70)
					Endif
				#EndIf
			
			Elseif credits = True
				DrawImage(img_level, 0, 0)
				#REM
				If creditStep = 1
					DrawImage(img_credits1, 0, 0)
				Elseif creditStep = 2
					DrawImage(img_credits2, 0, 0)
				Elseif creditStep = 3
					DrawImage(img_credits3, 0, 0)
				Elseif creditStep = 4
					DrawImage(img_credits4, 0, 0)
				Elseif creditStep = 5
					DrawImage(img_credits5, 0, 0)
				Elseif creditStep = 6
					DrawImage(img_credits6, 0, 0)
				Endif
				#End
			Else
			
				If gameProgress = 1
					DrawImage(img_stage2, 0, 0)
					If Millisecs() >= gameProgressTimer + 3500
						gameProgress = 2
					Endif
					
				Elseif gameProgress = 3
				
					DrawImage(img_stage3, 0, 0)
					If Millisecs() >= gameProgressTimer + 3500
						gameProgress = 4
					Endif
					
				Elseif gameProgress = 5
					DrawImage(img_completed, 0, 0)
					If Millisecs() >= gameProgressTimer + 3500
						gameProgress = 6
					Endif
					
				Else
				
				DrawImage(img_stageSelect, 0, 0)
				
				#If TARGET <> "android"
					'Draw Level Selector
					'Print selector
					If selector <> 9999
						Local x:Float = 0.0
						Local y:Float = 0.0
						If selector = 1
							x = 218.0
							y = 84.0
						Elseif selector = 2
							x = 650.0
							y = 84.0
						Elseif selector = 3
							x = 1080.0
							y = 84.0
						Elseif selector = 4
							x = 1514.0
							y = 84.0
						Elseif selector = 5
							x = 216.0
							y = 573.0
						Elseif selector = 6
							x = 648.0
							y = 573.0
						Elseif selector = 7
							x = 1080.0
							y = 573.0
						Elseif selector = 8
							x = 1512.0
							y = 573.0
						Elseif selector = 9
							x = 40.0
							y = 417.0
						Elseif selector = 10
							x = 1808.0
							y = 417.0
						Elseif selector = 11
							x = 0.0
							y = 943.0
						Elseif selector = 12
							x = 1540.0
							y = 943.0
						EndIf
						
						If selector < 9
							DrawImage(img_levelH, x, y)
						Elseif selector = 9
							DrawImage(img_leftArrowH, x, y)
						Elseif selector = 10
							DrawImage(img_rightArrowH, x, y)
						Elseif selector = 11
							DrawImage(img_menuH1, x, y)
						Elseif selector = 12
							DrawImage(img_optionsH1, x, y)
						Endif
					Endif
				#EndIf
				
				If stage = 1
					
					'Remove Left Arrow
					DrawRect(30, 400, 100, 165)
					
				Elseif stage = 3
					
					'Remove Right Arrow
					DrawRect(1790, 400, 100, 165)
					
				Endif
					
				'Draw Numbers on unlocked levels
				Local x:Float = 325.0
				Local y:Float = 125.0
				Local sx:Float = 243.0
				Local sy:Float = 315.0
				Local add:Int = (stage - 1) * 8
				For Local i:Int = 0 To 7
					If gameData.stage[stage - 1].level[i].unlocked = True
						DrawRect(x - 50, y + 5, 90, 135)
						#If TARGET <> "android"
							If selector <> (i + 1)
								fnt_stageSelectFont.DrawText(gameData.stage[stage - 1].level[i].ID + 1 + add, x, y, eDrawAlign.CENTER)
							Else
								fnt_stageSelectFontH.DrawText(gameData.stage[stage - 1].level[i].ID + 1 + add, x, y, eDrawAlign.CENTER)
							Endif
						#Else
							fnt_stageSelectFont.DrawText(gameData.stage[stage - 1].level[i].ID + 1 + add, x, y, eDrawAlign.CENTER)
						#EndIf
					Endif
					Local stars:Int = gameData.stage[stage - 1].level[i].starsEarned
					#If TARGET <> "android"
						If selector <> (i + 1)
							If stars = 1
								DrawImage(img_starFull, sx, sy)
							Elseif stars = 2
								DrawImage(img_starFull, sx, sy)
								DrawImage(img_starFull, sx + 58, sy)
							Elseif stars = 3
								DrawImage(img_starFull, sx, sy)
								DrawImage(img_starFull, sx + 58, sy)
								DrawImage(img_starFull, sx + 116, sy)
							Endif
						Else
							If stars = 1
								DrawImage(img_starFullH, sx, sy)
							Elseif stars = 2
								DrawImage(img_starFullH, sx, sy)
								DrawImage(img_starFullH, sx + 58, sy)
							Elseif stars = 3
								DrawImage(img_starFullH, sx, sy)
								DrawImage(img_starFullH, sx + 58, sy)
								DrawImage(img_starFullH, sx + 116, sy)
							Endif
						Endif
					#Else
						If stars = 1
							DrawImage(img_starFull, sx, sy)
						Elseif stars = 2
							DrawImage(img_starFull, sx, sy)
							DrawImage(img_starFull, sx + 58, sy)
						Elseif stars = 3
							DrawImage(img_starFull, sx, sy)
							DrawImage(img_starFull, sx + 58, sy)
							DrawImage(img_starFull, sx + 116, sy)
						Endif
					#EndIf
					sx = sx + 432.0
					If sx > 1540.0
						sx = 243.0
						sy = 804.0
					Endif
						
					x = x + 432.0
					If x > 1621
						x = 325
						y = 614
					Endif
				Next
				
				
				fnt_stageFont.DrawText("STAGE " + stage, 960, 435, eDrawAlign.CENTER)
				
				EndIf
				
			Endif
			
		Endif
		
		'Print VTouchX() + ", " + VTouchY()
		'Print player.body.GetAngle()

		'Return 0
		Return 0
		
	End Method
	
	Method LoadLevelBackground:Void(l:int)
		img_level.Discard()
		If stage = 1
			'Draw Level
			If l = 1
				img_level = LoadImage("Graphics/Levels/Level1.png")
			Elseif l = 2
				img_level = LoadImage("Graphics/Levels/Level2.png")
			ElseIf l = 3
				img_level = LoadImage("Graphics/Levels/Level3.png")
			Elseif l = 4
				img_level = LoadImage("Graphics/Levels/Level4.png")
			Elseif l = 5
				img_level = LoadImage("Graphics/Levels/Level5.png")
			Elseif l = 6
				img_level = LoadImage("Graphics/Levels/Level6.png")
			Elseif l = 7
				img_level = LoadImage("Graphics/Levels/Level7.png")
			Elseif l = 8
				img_level = LoadImage("Graphics/Levels/Level8.png")
			Endif
		ElseIf stage = 2
			'Draw Level
			If l = 1
				img_level = LoadImage("Graphics/Levels/Level9.png")
			Elseif l = 2
				img_level = LoadImage("Graphics/Levels/Level10.png")
			ElseIf l = 3
				img_level = LoadImage("Graphics/Levels/Level11.png")
			Elseif l = 4
				img_level = LoadImage("Graphics/Levels/Level1.png")
			Elseif l = 5
				img_level = LoadImage("Graphics/Levels/Level13.png")
			Elseif l = 6
				img_level = LoadImage("Graphics/Levels/Level14.png")
			Elseif l = 7
				img_level = LoadImage("Graphics/Levels/Level1.png")
			Elseif l = 8
				img_level = LoadImage("Graphics/Levels/Level1.png")
			Endif
		ElseIf stage = 3
			'Draw Level
			If l = 1
				img_level = LoadImage("Graphics/Levels/Level17.png")
			Elseif l = 2
				img_level = LoadImage("Graphics/Levels/Level18.png")
			ElseIf l = 3
				img_level = LoadImage("Graphics/Levels/Level19.png")
			Elseif l = 4
				img_level = LoadImage("Graphics/Levels/Level20.png")
			Elseif l = 5
				img_level = LoadImage("Graphics/Levels/Level8.png")
			Elseif l = 6
				img_level = LoadImage("Graphics/Levels/Level22.png")
			Elseif l = 7
				img_level = LoadImage("Graphics/Levels/Level23.png")
			Elseif l = 8
				img_level = LoadImage("Graphics/Levels/Level1.png")
			Endif
		Endif
		
	End Method
	
	Method DrawLevel:Void(l:Int)
	
		'Draw Level
		DrawImage(img_level, 0, 0)
		
		#Rem
		If stage = 1
			'Draw Level
			If l = 1
				DrawImage(img_level1, 0, 0)
			Elseif l = 2
				DrawImage(img_level2, 0, 0)
			ElseIf l = 3
				DrawImage(img_level3, 0, 0)
			Elseif l = 4
				DrawImage(img_level4, 0, 0)
			Elseif l = 5
				DrawImage(img_level5, 0, 0)
			Elseif l = 6
				DrawImage(img_level6, 0, 0)
			Elseif l = 7
				DrawImage(img_level7, 0, 0)
			Elseif l = 8
				DrawImage(img_level8, 0, 0)
			Endif
		ElseIf stage = 2
			'Draw Level
			If l = 1
				DrawImage(img_level9, 0, 0)
			Elseif l = 2
				DrawImage(img_level10, 0, 0)
			ElseIf l = 3
				DrawImage(img_level11, 0, 0)
			Elseif l = 4
				DrawImage(img_level1, 0, 0)
			Elseif l = 5
				DrawImage(img_level13, 0, 0)
			Elseif l = 6
				DrawImage(img_level14, 0, 0)
			Elseif l = 7
				DrawImage(img_level1, 0, 0)
			Elseif l = 8
				DrawImage(img_level1, 0, 0)
			Endif
		ElseIf stage = 3
			'Draw Level
			If l = 1
				DrawImage(img_level17, 0, 0)
			Elseif l = 2
				DrawImage(img_level18, 0, 0)
			ElseIf l = 3
				DrawImage(img_level19, 0, 0)
			Elseif l = 4
				DrawImage(img_level20, 0, 0)
			Elseif l = 5
				DrawImage(img_level8, 0, 0)
			Elseif l = 6
				DrawImage(img_level22, 0, 0)
			Elseif l = 7
				DrawImage(img_level23, 0, 0)
			Elseif l = 8
				DrawImage(img_level1, 0, 0)
			Endif
		EndIf
		#END
		
		'Draw Redo Button
		#If TARGET <> "android"
			If levelComplete = False
				If selector = 1
					DrawImage(img_redoH, 25, 25)
				Else
					DrawImage(img_redo, 25, 25)
				Endif
			Else
				DrawImage(img_redo, 25, 25)
			EndIf
		#Else
			
			DrawImage(img_redo, 25, 25)
			
		#EndIf
			
		'Draw Exit
		DrawImage(img_exit, exitX, exitY)

		
		'Draw Menu
		#If TARGET <> "android"
			If levelComplete = False
				If selector = 2
					DrawImage(img_menuH2, 1715, 25)
				Else
					DrawImage(img_menu, 1715, 25)
				Endif
			Else
				DrawImage(img_menu, 1715, 25)
			Endif
		#Else
			DrawImage(img_menu, 1715, 25)
		#EndIf
		
	End Method
	
	Method ResetPlayer:Void(l:Int)
		player.Kill()
		If stage = 0
			If l = 1
				player = Self.world.CreateImageBox(Self.img_player, 960, 540, False, 0.89, playerFriction, 5000, True)
			Elseif l = 2
				player = Self.world.CreateImageBox(Self.img_player, 400, 593, False, 0.89, playerFriction, 5000, True)
			EndIf
		ElseIf stage = 1
			If l = 1
				player = Self.world.CreateImageBox(Self.img_player, 400, 593, False, 0.89, playerFriction, 5000)
			ElseIf l = 2
				player = Self.world.CreateImageBox(Self.img_player, 320, 593, False, 0.89, playerFriction, 5000)
			ElseIf l = 3
				player = Self.world.CreateImageBox(Self.img_player, 180, 476, False, 0.89, playerFriction, 5000)
			ElseIf l = 4
				player = Self.world.CreateImageBox(Self.img_player, 320, 593, False, 0.89, playerFriction, 5000)
			Elseif l = 5
				player = Self.world.CreateImageBox(Self.img_player, 400, 200, False, 0.89, playerFriction, 5000)
			Elseif l = 6
				player = Self.world.CreateImageBox(Self.img_player, 960, 140, False, 0.83, playerFriction, 5000)
				player.body.SetLinearVelocity(New b2Vec2(0.0, 1.2))
			Elseif l = 7
				player = Self.world.CreateImageBox(Self.img_player, 260, 200, False, 0.89, playerFriction, 5000)
			Elseif l = 8
				player = Self.world.CreateImageBox(Self.img_player, 400, 520, False, 0.89, playerFriction, 5000)
			Endif
		Elseif stage = 2
			If l = 1
				player = Self.world.CreateImageBox(Self.img_player, 320, 530, False, 0.89, playerFriction, 5000)
				player.body.SetLinearVelocity(New b2Vec2(0.0, 5.0))
			ElseIf l = 2
				player = Self.world.CreateImageBox(Self.img_player, 60, 540, False, 0.89, playerFriction, 5000)
				player.body.SetLinearVelocity(New b2Vec2(2.0, 0.0))
			ElseIf l = 3
				player = Self.world.CreateImageBox(Self.img_player, 1400, 560, False, .89, playerFriction, 5000)
				player.body.SetAngle(1.575)
				player.body.SetLinearVelocity(New b2Vec2(4.0, 0.0))
			ElseIf l = 4
				player = Self.world.CreateImageBox(Self.img_player, 1700, 590, False, .89, playerFriction, 5000)
				player.body.SetAngle(1.575)
				player.body.SetLinearVelocity(New b2Vec2(-10.0, 0.0))
			Elseif l = 5
				player = Self.world.CreateImageBox(Self.img_player, 1225, 700, False, 0.89, playerFriction, 5000)
				player.body.SetAngle(-0.785)
				player.body.SetLinearVelocity(New b2Vec2(3.0, 3.0))
			Elseif l = 6
				player = Self.world.CreateImageBox(Self.img_player, 580, 140, False, 0.83, playerFriction, 5000)
				player.body.SetAngle(-1.285)
				player.body.SetLinearVelocity(New b2Vec2(6.0, 2.0))
			Elseif l = 7
				player = Self.world.CreateImageBox(Self.img_player, 400, 540, False, 0.89, playerFriction, 5000)
				player.body.SetAngle(1.575)
				player.body.SetLinearVelocity(New b2Vec2(-3.5, 0.0))
			Elseif l = 8
				player = Self.world.CreateImageBox(Self.img_player, 400, 540, False, 0.89, playerFriction, 5000)
				player.body.SetAngle(1.575)
				player.body.SetLinearVelocity(New b2Vec2(-4.5, 0.0))
			Endif
			
		Elseif stage = 3
			If l = 1
				player = Self.world.CreateImageBox(Self.img_player, 250, 520, False, 0.89, playerFriction, 5000)
			ElseIf l = 2
				player = Self.world.CreateImageBox(Self.img_player, 1500, 140, False, 0.89, playerFriction, 5000)
			ElseIf l = 3
				player = Self.world.CreateImageBox(Self.img_player, 500, -114, False, 0.89, playerFriction, 5000)
			ElseIf l = 4
				player = Self.world.CreateImageBox(Self.img_player, 400, 200, False, 0.89, playerFriction, 5000)
			Elseif l = 5
				player = Self.world.CreateImageBox(Self.img_player, 400, 520, False, 0.89, playerFriction, 5000)
			Elseif l = 6
				player = Self.world.CreateImageBox(Self.img_player, 300, 396, False, 0.89, playerFriction, 5000)
			Elseif l = 7
				player = Self.world.CreateImageBox(Self.img_player, 400, 593, False, 0.89, playerFriction, 5000)
			Elseif l = 8
				player = Self.world.CreateImageBox(Self.img_player, 300, 593, False, 0.89, playerFriction, 5000)
			Endif
			
		Endif
        
		'player.body.SetAngle(0.0)
		levelStartTimer = Millisecs()
		ResetBarriers(l)
		CreateExit(l)
		
	End Method
	
	Method ResetBarriers:Void(l:Int)
		For Local b:Barrier = Eachin barrierList
			b.ent.Kill()
		Next
		barrierList.Clear()
		
		If stage = 1
		
			If l = 5
				barrierList.AddLast(New Barrier(world, img_cross1, 960, 350, 600, 135, 600, 946, 4, 0.03))
			ElseIf l = 6
				barrierList.AddLast(New Barrier(world, img_barrierH, 234, 1000, 134, 1000, 600, 1000, 2))
				barrierList.AddLast(New Barrier(world, img_barrierH, 1686, 1000, 1320, 1000, 1786, 1000, 3))
			Elseif l = 7
				Local x:Float = 475.0
				Local y:Float = 622.0
				For Local i:Int = 0 To 6
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 33.0
				Next
				x = 1180.0
				y = 1044.0
				For Local i:Int = 0 To 5
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 32.0
				Next
				barrierList.AddLast(New Barrier(world, img_cross1, 770, 860, 600, 135, 600, 946, 4, 0.03))
			Elseif l = 8
				barrierList.AddLast(New Barrier(world, img_barrierV, 710, 472, 600, 135, 600, 946, 0))
				barrierList.AddLast(New Barrier(world, img_barrierV, 1210, 608, 600, 135, 600, 946, 1))
			Endif

		Elseif stage = 2
			If l = 1
				Local x:Float = 725.0
				Local y:Float = 641.0
				For Local i:Int = 0 To 6
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 33.0
				Next
				x = 1213.0
				y = 642.0
				For Local i:Int = 0 To 6
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 32.0
				Next
				
			Elseif l = 2
				
				barrierList.AddLast(New Barrier(world, img_barrierV, 780, 135, 600, 135, 600, 946, 0))
				barrierList.AddLast(New Barrier(world, img_barrierV, 1336, 595, 600, 135, 600, 946, 0))
				
			Elseif l = 3
			
				'barrierList.AddLast(New Barrier(world, img_barrierV, 883, 650, 600, 275, 600, 946, 0))
			Elseif l = 4
			
				Local x:Float = 200.0
				Local y:Float = 240.0
				Repeat
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					x = x + 100.0
					If x > 1700.0
						y = y + 100.0
						x = 200.0
					Endif
				Until y > 900.0
					
			ElseIf l = 5

				
				#rem LEGACY
				Local x:Float = 480.0
				Local y:Float = 568.0
				For Local i:Int = 0 To 6
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 32.0
				Next
				x = 954.0
				y = 646.0
				For Local i:Int = 0 To 6
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 32.0
				Next
				x = 1377.0
				y = 434.0
				For Local i:Int = 0 To 6
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 32.0
				Next
				#end

			ElseIf l = 6
				barrierList.AddLast(New Barrier(world, img_barrierV, 400, 115.5, 600, 115.5, 600, 964.5, 0, 3.2))
				barrierList.AddLast(New Barrier(world, img_barrierV, 1580, 145, 600, 115.5, 600, 964.5, 1, 3.2))
			Elseif l = 7
				barrierList.AddLast(New Barrier(world, img_barrierV, 700, 360, 600, 360, 600, 760, 0, 3.2))
				barrierList.AddLast(New Barrier(world, img_barrierV, 1220, 760, 600, 360, 600, 760, 1, 3.2))
				barrierList.AddLast(New Barrier(world, img_barrierH, 815.5, 231, 815.5, 144.5, 1104.5, 144.5, 2, 2.32))
				barrierList.AddLast(New Barrier(world, img_barrierH, 1104.5, 889, 815.5, 975.5, 1104.5, 975.5, 3, 2.32))
			Elseif l = 8
				barrierList.AddLast(New Barrier(world, img_cross1, 960, 540.0, 600, 135, 600, 946, 4, 0.03))
				
				Local x:Float = 720.0
				Local y:Float = 235.0
				For Local i:Int = 0 To 6
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 32.0
				Next
				
				x = 1200.0
				y = 235.0
				For Local i:Int = 0 To 6
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 32.0
				Next
				
				x = 1200.0
				y = 1037.0
				For Local i:Int = 0 To 6
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 32.0
				Next
				
				x = 720.0
				y = 1037.0
				For Local i:Int = 0 To 6
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 32.0
				Next
			Endif
			
		Elseif stage = 3
			
			If l = 1
				barrierList.AddLast(New Barrier(world, img_barrierH, 475.5, 1020, 475.5, 1558.5, 1443.5, 1558.5, 2, 2.32))
			Elseif l = 2
				barrierList.AddLast(New Barrier(world, img_cross1, 400, 580, 600, 135, 600, 946, 4, 0.03))
			Elseif l = 3
			
			Elseif l = 4
			
			Elseif l = 5
				barrierList.AddLast(New Barrier(world, img_cross1, 675, 900, 600, 135, 600, 946, 4, 0.03))
				barrierList.AddLast(New Barrier(world, img_cross1, 1260, 900, 600, 135, 600, 946, 4, 0.03))
			Elseif l = 6
			
			Elseif l = 7
			
			Elseif l = 8
				barrierList.AddLast(New Barrier(world, img_barrierV, 710, 250, 600, 135, 600, 946, 1))
				barrierList.AddLast(New Barrier(world, img_barrierV, 1210, 830, 600, 135, 600, 946, 0))
				
				Local x:Float = 475.0
				Local y:Float = 1044.0
				For Local i:Int = 0 To 6
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 32.0
				Next
				x = 960.0
				y = 1044.0
				For Local i:Int = 0 To 5
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 32.0
				Next
				x = 1445.0
				y = 1044.0
				For Local i:Int = 0 To 5
					barrierList.AddLast(New Barrier(world, img_cube1, x, y, 0, 0, 0, 0, 6))
					y = y - 32.0
				Next
				
			Endif
			
		EndIf
	End Method
	
	Method CreateExit:Void(x:Float, y:Float)
		Local sensor:b2Body
        Local box:b2PolygonShape = New b2PolygonShape()
        box.SetAsBox(50.0/64.0, 25.0/64.0)
        Local fd:b2FixtureDef = New b2FixtureDef()
        fd.shape = box
        fd.density = 4
        fd.friction = 0.4
        fd.restitution = 0.3
        fd.userData = New StringObject("sensor")
        fd.isSensor = True
        Local bd :b2BodyDef = New b2BodyDef()
        bd.type = b2Body.b2_staticBody
        bd.position.Set((x + 50)/64.0, (y+25)/64.0)
        sensor = world.world.CreateBody(bd)
        sensor.CreateFixture(fd)
        world.world.SetContactListener(New SensorContactListener(sensor, player.body))
        exitX = x
        exitY = y
	End Method'
	
	Method SetResolution:Void(width:Int, height:Int, fullscreen:Bool = False)
	
		#rem
		'Auto Reload
		AddAutoLoadImage(GetFont())
		AddAutoLoadImage(img_player)
		AddAutoLoadImage(img_mainMenu)
		AddAutoLoadImage(img_stageSelect)
		AddAutoLoadImage(img_levelComplete)
		AddAutoLoadImage(img_level1)
		AddAutoLoadImage(img_level2)
		AddAutoLoadImage(img_level3)
		AddAutoLoadImage(img_level4)
		AddAutoLoadImage(img_level5)
		AddAutoLoadImage(img_level6)
		AddAutoLoadImage(img_level7)
		AddAutoLoadImage(img_level8)
		AddAutoLoadImage(img_level9)
		AddAutoLoadImage(img_level10)
		AddAutoLoadImage(img_level11)
		AddAutoLoadImage(img_level12)
		AddAutoLoadImage(img_level13)
		AddAutoLoadImage(img_level14)
		AddAutoLoadImage(img_level15)
		AddAutoLoadImage(img_level16)
		AddAutoLoadImage(img_level17)
		AddAutoLoadImage(img_level18)
		AddAutoLoadImage(img_level19)
		AddAutoLoadImage(img_level20)
		AddAutoLoadImage(img_level21)
		AddAutoLoadImage(img_level22)
		AddAutoLoadImage(img_level23)
		AddAutoLoadImage(img_level24)
		AddAutoLoadImage(img_redo)
		AddAutoLoadImage(img_menu)
		AddAutoLoadImage(img_exit)
		AddAutoLoadImage(img_starFull)
		AddAutoLoadImage(img_starFullLarge)
		AddAutoLoadImage(img_levelH)
		AddAutoLoadImage(img_starFullH)
		AddAutoLoadImage(img_rightArrowH)
		AddAutoLoadImage(img_leftArrowH)
		AddAutoLoadImage(img_menuH1)
		AddAutoLoadImage(img_optionsH1)
		AddAutoLoadImage(img_playH)
		AddAutoLoadImage(img_tutorialH)
		AddAutoLoadImage(img_optionsH2)
		AddAutoLoadImage(img_menuH2)
		AddAutoLoadImage(img_menuH3)
		AddAutoLoadImage(img_nextH)
		AddAutoLoadImage(img_redoH)
		AddAutoLoadImage(img_barrierV)
		AddAutoLoadImage(img_barrierH)
		AddAutoLoadImage(img_cross1)
		AddAutoLoadImage(img_cube1)
		AddAutoLoadImage(img_options)
		AddAutoLoadImage(img_res1H)
		AddAutoLoadImage(img_res2H)
		AddAutoLoadImage(img_res3H)
		AddAutoLoadImage(img_windowedH)
		AddAutoLoadImage(img_fullscreenH)
		AddAutoLoadImage(img_creditsH)
		AddAutoLoadImage(img_exitGameH)
		AddAutoLoadImage(img_returnH)
		
		fnt_stageSelectFont.UnloadFullFont()
		
		ChangeScreenMode(width, height, 32, fullscreen)
		
		ClearAutoLoadImages()
		
		fnt_stageFont = New BitmapFont("Graphics/Fonts/Stage_Font.txt", False)
		fnt_stageSelectFont = New BitmapFont("Graphics/Fonts/Stage_Select_Font.txt", False)
		fnt_stageSelectFontH = New BitmapFont("Graphics/Fonts/Stage_Select_FontH.txt", False)
		fnt_72Font = New BitmapFont("Graphics/Fonts/72Font.txt", False)
		fnt_54Font = New BitmapFont("Graphics/Fonts/54Font.txt", False)
		fnt_timeFont = New BitmapFont("Graphics/Fonts/Time_Font.txt", False)
		
		#end
		
		SetDeviceWindow(width, height, fullscreen)
		
		If gamepad = False
			ShowMouse()
		Endif
		
	End Method
	
	Method CreateLevel3:Entity()
	
	    'Set scale
	    Local scale:Float = 64.0
	
	    'Create Polygon List
	    Local pList:List<Polygon> = New List<Polygon>
	
	    'Create vertices array
	    Local vertices:b2Vec2[]
	
	    'Polygon1
	    vertices = New b2Vec2[4]
	    vertices[0] = New b2Vec2(-356.000/scale, 540.000/scale)
	    vertices[1] = New b2Vec2(-958.000/scale, 423.000/scale)
	    vertices[2] = New b2Vec2(-938.000/scale, 403.000/scale)
	    vertices[3] = New b2Vec2(-356.000/scale, 403.000/scale)
	    pList.AddLast(New Polygon(vertices, 4))
	
	    'Polygon2
	    vertices = New b2Vec2[4]
	    vertices[0] = New b2Vec2(942.000/scale, 403.000/scale)
	    vertices[1] = New b2Vec2(962.000/scale, 423.000/scale)
	    vertices[2] = New b2Vec2(36.000/scale, 540.000/scale)
	    vertices[3] = New b2Vec2(36.000/scale, 403.000/scale)
	    pList.AddLast(New Polygon(vertices, 4))
	
	    'Polygon3
	    vertices = New b2Vec2[4]
	    vertices[0] = New b2Vec2(942.000/scale, 403.000/scale)
	    vertices[1] = New b2Vec2(942.000/scale, -520.000/scale)
	    vertices[2] = New b2Vec2(962.000/scale, -540.000/scale)
	    vertices[3] = New b2Vec2(962.000/scale, 423.000/scale)
	    pList.AddLast(New Polygon(vertices, 4))
	
	    'Polygon4
	    vertices = New b2Vec2[4]
	    vertices[0] = New b2Vec2(-958.000/scale, 423.000/scale)
	    vertices[1] = New b2Vec2(-958.000/scale, -540.000/scale)
	    vertices[2] = New b2Vec2(-938.000/scale, -520.000/scale)
	    vertices[3] = New b2Vec2(-938.000/scale, 403.000/scale)
	    pList.AddLast(New Polygon(vertices, 4))
	
	    'Polygon5
	    vertices = New b2Vec2[4]
	    vertices[0] = New b2Vec2(942.000/scale, -520.000/scale)
	    vertices[1] = New b2Vec2(-938.000/scale, -520.000/scale)
	    vertices[2] = New b2Vec2(-958.000/scale, -540.000/scale)
	    vertices[3] = New b2Vec2(962.000/scale, -540.000/scale)
	    pList.AddLast(New Polygon(vertices, 4))
	
	    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))
	
	End Method
	
Method CreateLevel4:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-252.000/scale, 369.000/scale)
    vertices[1] = New b2Vec2(-252.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(-378.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(-378.000/scale, 369.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(173.000/scale, 369.000/scale)
    vertices[1] = New b2Vec2(173.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(47.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(47.000/scale, 369.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-752.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-940.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon6
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-735.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon7
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method


Method CreateLevel2:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-6.000/scale, 366.000/scale)
    vertices[1] = New b2Vec2(-6.000/scale, 521.000/scale)
    vertices[2] = New b2Vec2(-132.000/scale, 521.000/scale)
    vertices[3] = New b2Vec2(-132.000/scale, 366.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-729.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon3
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, 521.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 521.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-940.000/scale, 521.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-717.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon6
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, 521.000/scale)
    vertices[2] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method


Method CreateLevel1:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-754.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-700.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(940.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel5:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(111.000/scale, 394.000/scale)
    vertices[1] = New b2Vec2(111.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-111.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-111.000/scale, 394.000/scale)
    pList.AddLast(New Polygon(vertices, 4))
    
    CreateLevel5_2()

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel5_2:Entity()

   'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(362.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(362.000/scale, 169.000/scale)
    vertices[2] = New b2Vec2(382.000/scale, 189.000/scale)
    vertices[3] = New b2Vec2(382.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 169.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 189.000/scale)
    vertices[2] = New b2Vec2(382.000/scale, 189.000/scale)
    vertices[3] = New b2Vec2(362.000/scale, 169.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 169.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, 189.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, 169.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 189.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(-940.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon6
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, 169.000/scale)
    vertices[1] = New b2Vec2(-362.000/scale, 169.000/scale)
    vertices[2] = New b2Vec2(-382.000/scale, 189.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, 189.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon7
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-362.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(-382.000/scale, 539.000/scale)
    vertices[2] = New b2Vec2(-382.000/scale, 189.000/scale)
    vertices[3] = New b2Vec2(-362.000/scale, 169.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon8
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(-382.000/scale, 539.000/scale)
    vertices[1] = New b2Vec2(-362.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))


End Method


Method CreateLevel6:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(940.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[3] = New b2Vec2(-940.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))
    
    CreateLevel6_2()
    CreateLevel6_3()

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel6_2:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(246.000/scale, -72.000/scale)
    vertices[1] = New b2Vec2(246.000/scale, 24.000/scale)
    vertices[2] = New b2Vec2(-245.000/scale, 24.000/scale)
    vertices[3] = New b2Vec2(-245.000/scale, -72.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel6_3:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(246.000/scale, 368.000/scale)
    vertices[1] = New b2Vec2(246.000/scale, 640.000/scale)
    vertices[2] = New b2Vec2(-245.000/scale, 640.000/scale)
    vertices[3] = New b2Vec2(-245.000/scale, 368.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method
	
	
Method CreateLevel7:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-376.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, 99.000/scale)
    vertices[3] = New b2Vec2(-376.000/scale, 99.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(258.000/scale, -521.000/scale)
    vertices[1] = New b2Vec2(361.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(361.000/scale, 61.000/scale)
    vertices[3] = New b2Vec2(258.000/scale, 61.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(364.000/scale, -521.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(940.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(258.000/scale, -521.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(361.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon6
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, 99.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(-940.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon7
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method


Method CreateLevel8:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-420.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, 452.000/scale)
    vertices[3] = New b2Vec2(-420.000/scale, 452.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 452.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(420.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(420.000/scale, 452.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(326.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(940.000/scale, 452.000/scale)
    vertices[3] = New b2Vec2(940.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(312.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon6
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, 452.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(-940.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon7
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(420.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-420.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method


Method CreateLevel9:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[6]
    vertices[0] = New b2Vec2(344.000/scale, 119.000/scale)
    vertices[1] = New b2Vec2(344.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(324.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(157.000/scale, 540.000/scale)
    vertices[4] = New b2Vec2(137.000/scale, 520.000/scale)
    vertices[5] = New b2Vec2(137.000/scale, 119.000/scale)
    pList.AddLast(New Polygon(vertices, 6))

    'Polygon2
    vertices = New b2Vec2[6]
    vertices[0] = New b2Vec2(-138.000/scale, 119.000/scale)
    vertices[1] = New b2Vec2(-138.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(-158.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-325.000/scale, 540.000/scale)
    vertices[4] = New b2Vec2(-345.000/scale, 520.000/scale)
    vertices[5] = New b2Vec2(-345.000/scale, 119.000/scale)
    pList.AddLast(New Polygon(vertices, 6))

    'Polygon3
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-27.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(-158.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-138.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(-27.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(137.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(157.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-16.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-26.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-325.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(-345.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon6
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(324.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(344.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon7
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -160.000/scale)
    vertices[2] = New b2Vec2(960.000/scale, -170.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon8
    vertices = New b2Vec2[5]
    vertices[0] = New b2Vec2(580.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(588.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(590.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, -170.000/scale)
    vertices[4] = New b2Vec2(940.000/scale, -160.000/scale)
    pList.AddLast(New Polygon(vertices, 5))

    'Polygon9
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-960.000/scale, -170.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, -160.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon10
    vertices = New b2Vec2[5]
    vertices[0] = New b2Vec2(-960.000/scale, -170.000/scale)
    vertices[1] = New b2Vec2(-590.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-588.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(-580.000/scale, -520.000/scale)
    vertices[4] = New b2Vec2(-940.000/scale, -160.000/scale)
    pList.AddLast(New Polygon(vertices, 5))

    'Polygon11
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(580.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-580.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-588.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(588.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel10:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(216.000/scale, 128.000/scale)
    vertices[1] = New b2Vec2(216.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(116.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(116.000/scale, 128.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(116.000/scale, -126.000/scale)
    vertices[1] = New b2Vec2(116.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(216.000/scale, -520.000/scale)
    vertices[3] = New b2Vec2(216.000/scale, -126.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(516.000/scale, -80.000/scale)
    vertices[1] = New b2Vec2(516.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(616.000/scale, -520.000/scale)
    vertices[3] = New b2Vec2(616.000/scale, -80.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-526.000/scale, -77.000/scale)
    vertices[1] = New b2Vec2(-526.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-426.000/scale, -520.000/scale)
    vertices[3] = New b2Vec2(-426.000/scale, -77.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-426.000/scale, 77.000/scale)
    vertices[1] = New b2Vec2(-426.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(-526.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(-526.000/scale, 77.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon6
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(616.000/scale, 77.000/scale)
    vertices[1] = New b2Vec2(616.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(516.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(516.000/scale, 77.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon7
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(960.000/scale, 414.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(940.000/scale, 414.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon8
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-426.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(940.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon9
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(960.000/scale, 394.000/scale)
    vertices[3] = New b2Vec2(940.000/scale, 413.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon10
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-426.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(940.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon11
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-426.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon12
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon13
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(-426.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel11:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-774.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(-774.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[3] = New b2Vec2(-940.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-756.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-773.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

	CreateLevel11_2()
	
    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel11_2:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(40.000/scale, -154.000/scale)
    vertices[1] = New b2Vec2(40.000/scale, 155.000/scale)
    vertices[2] = New b2Vec2(-40.000/scale, 155.000/scale)
    vertices[3] = New b2Vec2(-40.000/scale, -154.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method


Method CreateLevelb2:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-119.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-99.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-99.000/scale, -474.000/scale)
    vertices[3] = New b2Vec2(-119.000/scale, -474.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(442.000/scale, -139.000/scale)
    vertices[1] = New b2Vec2(442.000/scale, 281.000/scale)
    vertices[2] = New b2Vec2(330.000/scale, 180.000/scale)
    vertices[3] = New b2Vec2(330.000/scale, -139.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-119.000/scale, 306.000/scale)
    vertices[1] = New b2Vec2(-119.000/scale, 180.000/scale)
    vertices[2] = New b2Vec2(-37.000/scale, 281.000/scale)
    vertices[3] = New b2Vec2(-37.000/scale, 306.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-35.000/scale, -119.000/scale)
    vertices[1] = New b2Vec2(-37.000/scale, 281.000/scale)
    vertices[2] = New b2Vec2(-119.000/scale, 180.000/scale)
    vertices[3] = New b2Vec2(-119.000/scale, -119.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-37.000/scale, 281.000/scale)
    vertices[1] = New b2Vec2(-35.000/scale, 180.000/scale)
    vertices[2] = New b2Vec2(330.000/scale, 180.000/scale)
    vertices[3] = New b2Vec2(442.000/scale, 281.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon6
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-119.000/scale, 281.000/scale)
    vertices[1] = New b2Vec2(-611.000/scale, 281.000/scale)
    vertices[2] = New b2Vec2(-515.000/scale, 180.000/scale)
    vertices[3] = New b2Vec2(-119.000/scale, 180.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon7
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-611.000/scale, -57.000/scale)
    vertices[1] = New b2Vec2(-515.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-515.000/scale, 180.000/scale)
    vertices[3] = New b2Vec2(-611.000/scale, 281.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon8
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, -57.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-515.000/scale, -520.000/scale)
    vertices[3] = New b2Vec2(-611.000/scale, -57.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon9
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-99.000/scale, -473.000/scale)
    vertices[1] = New b2Vec2(8.000/scale, -459.000/scale)
    vertices[2] = New b2Vec2(-119.000/scale, -375.000/scale)
    vertices[3] = New b2Vec2(-119.000/scale, -473.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon10
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(8.000/scale, -459.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(744.000/scale, -375.000/scale)
    vertices[3] = New b2Vec2(-119.000/scale, -375.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon11
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -178.000/scale)
    vertices[2] = New b2Vec2(744.000/scale, -375.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon12
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-515.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-99.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(-119.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon13
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, -57.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, 380.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon14
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(-800.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, 380.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon15
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, 303.000/scale)
    vertices[2] = New b2Vec2(940.000/scale, -178.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon16
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(722.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(940.000/scale, 303.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon17
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(722.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-800.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel13:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-41.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-481.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(-41.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, 60.000/scale)
    vertices[2] = New b2Vec2(-481.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon3
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(480.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-40.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-40.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon4
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(940.000/scale, 60.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(480.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon5
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 60.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -59.000/scale)
    vertices[2] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon6
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, -61.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, 60.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon7
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(-481.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, -61.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon8
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(478.000/scale, -520.000/scale)
    vertices[3] = New b2Vec2(-481.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon9
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -59.000/scale)
    vertices[2] = New b2Vec2(478.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

	CreateLevel13_2()
	CreateLevel13_3
	
    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel13_2:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-207.000/scale, -44.000/scale)
    vertices[1] = New b2Vec2(-42.000/scale, -208.000/scale)
    vertices[2] = New b2Vec2(0.000/scale, -165.000/scale)
    vertices[3] = New b2Vec2(-165.000/scale, 0.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel13_3:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(207.000/scale, 43.000/scale)
    vertices[1] = New b2Vec2(42.000/scale, 208.000/scale)
    vertices[2] = New b2Vec2(0.000/scale, 165.000/scale)
    vertices[3] = New b2Vec2(165.000/scale, 0.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel17:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 394.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(600.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(600.000/scale, 394.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-600.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, 394.000/scale)
    vertices[3] = New b2Vec2(-600.000/scale, 394.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 394.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, 394.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel18:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-654.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(-654.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-384.000/scale, 2.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, 2.000/scale)
    vertices[2] = New b2Vec2(940.000/scale, 76.000/scale)
    vertices[3] = New b2Vec2(-384.000/scale, 76.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-636.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[3] = New b2Vec2(-653.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(940.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon6
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel19:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-717.000/scale, -77.000/scale)
    vertices[1] = New b2Vec2(-149.000/scale, -77.000/scale)
    vertices[2] = New b2Vec2(-149.000/scale, 1.000/scale)
    vertices[3] = New b2Vec2(-717.000/scale, 1.000/scale)
    pList.AddLast(New Polygon(vertices, 4))
    
    CreateLevel19_2()
    CreateLevel19_3()

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel19_2:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-717.000/scale, 396.000/scale)
    vertices[1] = New b2Vec2(-149.000/scale, 396.000/scale)
    vertices[2] = New b2Vec2(-149.000/scale, 474.000/scale)
    vertices[3] = New b2Vec2(-717.000/scale, 474.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel19_3:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(311.000/scale, 493.000/scale)
    vertices[1] = New b2Vec2(255.000/scale, 438.000/scale)
    vertices[2] = New b2Vec2(657.000/scale, 36.000/scale)
    vertices[3] = New b2Vec2(713.000/scale, 92.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method


Method CreateLevel20:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 2.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(642.000/scale, 520.000/scale)
    vertices[3] = New b2Vec2(642.000/scale, 2.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(525.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, 2.000/scale)
    vertices[3] = New b2Vec2(525.000/scale, 2.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-250.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(-250.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, 2.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(-940.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-218.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[3] = New b2Vec2(-249.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon6
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(940.000/scale, 2.000/scale)
    vertices[3] = New b2Vec2(940.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon7
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(642.000/scale, 520.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel22:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-93.000/scale, 423.000/scale)
    vertices[1] = New b2Vec2(-93.000/scale, 209.000/scale)
    vertices[2] = New b2Vec2(93.000/scale, 209.000/scale)
    vertices[3] = New b2Vec2(93.000/scale, 423.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

	CreateLevel22_2()
	CreateLevel22_3()
	
    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel22_2:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-752.000/scale, 424.000/scale)
    vertices[1] = New b2Vec2(-752.000/scale, 210.000/scale)
    vertices[2] = New b2Vec2(-566.000/scale, 210.000/scale)
    vertices[3] = New b2Vec2(-566.000/scale, 424.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel22_3:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(566.000/scale, 424.000/scale)
    vertices[1] = New b2Vec2(566.000/scale, 210.000/scale)
    vertices[2] = New b2Vec2(752.000/scale, 210.000/scale)
    vertices[3] = New b2Vec2(752.000/scale, 424.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevel23:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-324.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(-307.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-940.000/scale, 474.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 494.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, 474.000/scale)
    vertices[1] = New b2Vec2(-335.000/scale, 474.000/scale)
    vertices[2] = New b2Vec2(-315.000/scale, 494.000/scale)
    vertices[3] = New b2Vec2(-960.000/scale, 494.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-335.000/scale, 474.000/scale)
    vertices[1] = New b2Vec2(-335.000/scale, 213.000/scale)
    vertices[2] = New b2Vec2(-315.000/scale, 233.000/scale)
    vertices[3] = New b2Vec2(-315.000/scale, 494.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-335.000/scale, 213.000/scale)
    vertices[1] = New b2Vec2(-164.000/scale, 213.000/scale)
    vertices[2] = New b2Vec2(-184.000/scale, 233.000/scale)
    vertices[3] = New b2Vec2(-315.000/scale, 233.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon6
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-164.000/scale, 323.000/scale)
    vertices[1] = New b2Vec2(-184.000/scale, 343.000/scale)
    vertices[2] = New b2Vec2(-184.000/scale, 233.000/scale)
    vertices[3] = New b2Vec2(-164.000/scale, 213.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon7
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-164.000/scale, 323.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, 323.000/scale)
    vertices[2] = New b2Vec2(960.000/scale, 343.000/scale)
    vertices[3] = New b2Vec2(-184.000/scale, 343.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon8
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 323.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, 343.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon9
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-306.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-306.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method


Method CreateLevelLEGACY:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-352.000/scale, 44.000/scale)
    vertices[1] = New b2Vec2(-352.000/scale, 520.000/scale)
    vertices[2] = New b2Vec2(-537.000/scale, 337.000/scale)
    vertices[3] = New b2Vec2(-537.000/scale, 44.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-352.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-940.000/scale, 337.000/scale)
    vertices[3] = New b2Vec2(-537.000/scale, 337.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[2] = New b2Vec2(-741.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(-741.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-940.000/scale, 337.000/scale)
    vertices[1] = New b2Vec2(-960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(-940.000/scale, -520.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon5
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(960.000/scale, 540.000/scale)
    vertices[2] = New b2Vec2(-960.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon6
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, 520.000/scale)
    vertices[1] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(960.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, 540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon7
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(940.000/scale, -520.000/scale)
    vertices[1] = New b2Vec2(-740.000/scale, -520.000/scale)
    vertices[2] = New b2Vec2(-733.000/scale, -540.000/scale)
    vertices[3] = New b2Vec2(960.000/scale, -540.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    CreateLevelLEGACY_2()
    CreateLevelLEGACY_3()

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevelLEGACY_2:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(102.000/scale, 122.000/scale)
    vertices[1] = New b2Vec2(102.000/scale, 342.000/scale)
    vertices[2] = New b2Vec2(-83.000/scale, 342.000/scale)
    vertices[3] = New b2Vec2(-83.000/scale, 122.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method

Method CreateLevelLEGACY_3:Entity()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(523.000/scale, -90.000/scale)
    vertices[1] = New b2Vec2(523.000/scale, 130.000/scale)
    vertices[2] = New b2Vec2(338.000/scale, 130.000/scale)
    vertices[3] = New b2Vec2(338.000/scale, -90.000/scale)
    pList.AddLast(New Polygon(vertices, 4))

    Return(Self.world.CreateMultiPolygon(960, 540, pList, True))

End Method


	
End Class


Class SensorContactListener Extends b2ContactListener
    Field sensor:b2Body
    Field player:b2Body

    Method New(sensor:b2Body, player:b2Body)
        Self.sensor = sensor
        Self.player = player
    End
    
    Method BeginContact:void (contact:b2Contact)
        If contact.GetFixtureA().GetBody() = sensor Or contact.GetFixtureB().GetBody() = sensor
        	If contact.GetFixtureA().GetBody() = player Or contact.GetFixtureB().GetBody() = player
            	Game.sensorColliding = True
            EndIf
        End
    End
    
    Method EndContact : void (contact:b2Contact)
        If contact.GetFixtureA().GetBody() = sensor Or contact.GetFixtureB().GetBody() = sensor
        	If contact.GetFixtureA().GetBody() = player Or contact.GetFixtureB().GetBody() = player
            	Game.sensorColliding = False
            EndIf
        End
    End
    
End Class

Class GameData
	
	Field stage:StageData[3]
	
	Method New()
		For Local i:int = 0 To 2
			Self.stage[i] = New StageData(i, True)
		Next
	End Method
	
	Method CompleteLevel:Void(stage:Int, level:Int, time:Float)
	
		Local s:Int = (stage - 1)
		Local l:Int = (level - 1)
		
		'Unlock next stage
		If l = 7
			If s < 2
				Self.stage[s + 1].level[0].unlocked = True
			EndIf
		Else
			Self.stage[s].level[l + 1].unlocked = True
		EndIf
		
		'Assign new best time if necessary
		If Self.stage[s].level[l].bestTime > time
			'Print "Old Best: " + Self.stage[s].level[l].bestTime
			Self.stage[s].level[l].bestTime = time
			'Print "New Best: " + Self.stage[s].level[l].bestTime
		Endif
		
		'Assign Stars
		Local tempStars:Int = AssignStars(stage, level, time)
		If Self.stage[s].level[l].starsEarned < tempStars Then Self.stage[s].level[l].starsEarned = tempStars
		
	End Method
	
	Method SaveString:String(progress:Int, music:Int)
		Local s:String = ""
		For Local i:Int = 0 To 2
			s = s + String(Self.stage[i].ID) + ","
			If Self.stage[i].unlocked = True
				s = s + "1,"
			Else
				s = s + "0,"
			Endif
			For Local l:Int = 0 To 7
				s = s + String(Self.stage[i].level[l].ID) + ","
				If Self.stage[i].level[l].unlocked = True
					s = s + "1,"
				Else
					s = s + "0,"
				Endif
				s = s + String(Self.stage[i].level[l].starsEarned) + ","
				s = s + String(Self.stage[i].level[l].bestTime) + ","
			Next
		Next
		s = s + currentVersionCode + ","
		s = s + String(progress) + ","
		s = s + String(music)
		Return(s)
	End Method
	
	Method LoadString:Void(st:String)
		Local s2:String[] = st.Split(",")
		Local i:Int = 0
		
			Local s:Int = 0
			For Local s:Int = 0 To 2	
				Self.stage[s].ID = Int(s2[i])
				i = i + 1
				If s2[i] = "1"
					Self.stage[s].unlocked = True
				Else
					Self.stage[s].unlocked = False
				Endif
				i = i + 1
				For Local l:Int = 0 To 7
					Self.stage[s].level[l].ID = Int(s2[i])
					i = i + 1
					If s2[i] = "1"
						Self.stage[s].level[l].unlocked = True
					Else
						Self.stage[s].level[l].unlocked = False
					Endif
					i = i + 1
					Self.stage[s].level[l].starsEarned = Int(s2[i])
					i = i + 1
					Self.stage[s].level[l].bestTime = Float(s2[i])
					i = i + 1
				Next
			Next
		
	End Method
	
End Class

Class LevelData
		
	Field ID:Int
	Field unlocked:Bool
	Field starsEarned:Int
	Field bestTime:Float
		
	Method New(id:Int, unlocked:Bool = False, starsEarned:Int = 0, bestTime:Float = 99999999.00)
		Self.ID = id
		Self.unlocked = unlocked
		Self.starsEarned = starsEarned
		Self.bestTime = bestTime
	End Method
		
End Class

Class StageData
	
	Field ID:Int
	Field unlocked:Bool
	Field level:LevelData[8]
		
	Method New(id:Int, unlocked:Bool = False)
		Self.ID = id
		Self.unlocked = unlocked
		For Local i:Int = 0 To 7
			Self.level[i] = New LevelData(i, False)
		Next
	End Method
			
End Class

Function AssignStars:Int(stage:Int, level:Int, time:Float)
	
	Local stars:Int = 1
	time = Float(time/1000)
	'Print time
	
	If stage = 1
	
		If level = 1
			stars = aStars(5.0, 7.0, time)	
		Elseif level = 2
			stars = aStars(7.0, 9.0, time)	
		Elseif level = 3
			stars = aStars(6.0, 8.0, time)	
		Elseif level = 4
			stars = aStars(6.0, 9.0, time)	
		Elseif level = 5
			stars = aStars(5.0, 8.0, time)	
		Elseif level = 6
			stars = aStars(4.0, 8.0, time)	
		Elseif level = 7
			stars = aStars(8.0, 11.0, time)	
		Elseif level = 8
			stars = aStars(7.0, 12.0, time)	
		EndIf
	
	Elseif stage = 2
	
		If level = 1
			stars = aStars(10.0, 14.0, time)	
		Elseif level = 2
			stars = aStars(9.0, 12.0, time)	
		Elseif level = 3
			stars = aStars(7, 10.5, time)	
		Elseif level = 4
			stars = aStars(3.0, 6.0, time)	
		Elseif level = 5
			stars = aStars(5.5, 9.5, time)	
		Elseif level = 6
			stars = aStars(6.5, 9.0, time)	
		Elseif level = 7
			stars = aStars(3.5, 6.5, time)	
		Elseif level = 8
			stars = aStars(9.5, 15.0, time)	
		EndIf
	
	Elseif stage = 3
	
		If level = 1
			stars = aStars(6.5, 7.5, time)	
		Elseif level = 2
			stars = aStars(25.0, 40.0, time)	
		Elseif level = 3
			stars = aStars(4.5, 6.0, time)	
		Elseif level = 4 'START HERE
			stars = aStars(9.5, 12.0, time)	
		Elseif level = 5
			stars = aStars(4.5, 8.0, time)	
		Elseif level = 6
			stars = aStars(7.1, 10.0, time)	
		Elseif level = 7
			stars = aStars(8.0, 13.0, time)	
		Elseif level = 8
			stars = aStars(7.0, 12.0, time)	
		Endif
		
	Endif
	
	Return(stars)
	
End Function

Function aStars:Int(gold:Float, silver:Int, time:Float)
	Local stars:Int = 1
	If time <= gold
		stars = 3
		'Print "Gold"
	Elseif time > gold And time <= silver
		stars = 2
		'Print "Silver"
	Else
		stars = 1
		'Print "Bronze"
	Endif
	Return(stars)
End Function

Class Barrier
	
	Field ent:Entity
	Field x:Float
	Field y:Float
	Field startX:Float
	Field startY:Float
	Field endX:Float
	Field endY:Float
	Field direction:Int
	Field speed:Float
	
	Method New(world:Box2D_World, img:Image, x:Float, y:Float, sX:Float, sY:Float, eX:Float, eY:Float, dir:Int = 0, speed:Float = 3.0, static:Bool = True)
		If dir < 4
			Self.ent = world.CreateImageBox(img, 400, 476, static, 0.89, 1, 1)
		Elseif dir = 4 Or dir = 5
			Self.ent = world.CreateMultiPolygon(960, 540, CreateCross1(), static)
			Self.ent.img = img
			Self.ent.body.SetAngle(45.0)
		Elseif dir = 6
			#If TARGET = "android"
				Self.ent = world.CreateImageBox(img, x, y, False, 0.20, 50000, 0.2) '50000000000000000000000000000, 0.2)
			#Else
				Self.ent = world.CreateImageBox(img, x, y, False, 0.20, 50000000000000000000000000000, 0.2)
			#Endif
		EndIf
		
		Self.x = x
		Self.y = y
		Self.startX = sX
		Self.startY = sY
		Self.endX = eX
		Self.endY = eY
		Self.direction = dir
		Self.speed = speed
	End Method
	
	Method Update:Void()
		If Self.direction = 0
			Self.y = Self.y + Self.speed
			If Self.y >= Self.endY 
				Self.direction = 1
			Endif
		Elseif Self.direction = 1
			Self.y = Self.y - Self.speed
			If Self.y <= Self.startY 
				Self.direction = 0
			Endif
		Endif
		
		If Self.direction = 2
			Self.x = Self.x + Self.speed
			If Self.x >= Self.endX
				Self.direction = 3
			Endif
		Elseif Self.direction = 3
			Self.x = Self.x - Self.speed
			If Self.x <= Self.startX 
				Self.direction = 2
			Endif
		Endif
		
		If Self.direction = 4
			Local a:Float = Self.ent.body.GetAngle()
			Self.ent.body.SetAngle(a + speed)
		Endif
		
		If Self.direction = 5
			Local a:Float = Self.ent.body.GetAngle()
			Self.ent.body.SetAngle(a - speed)
		Endif
		
		'Cube
		If Self.direction = 6
			
		Endif
		
		If Self.ent.bodyDef.type = b2Body.b2_staticBody
			Self.ent.body.SetPosition(New b2Vec2(Self.x/64, Self.y/64))
		Endif
		
	End Method
	
	Method Draw:Void()
	
	End Method
	
End Class

Class DoTweet
	Function LaunchTwitter:Void(twitter_name:String, twitter_text:String, hashtags:String = "")
		'OpenUrl("http://twitter.com/share?screen_name=" + twitter_name + "&text=" + twitter_text + "&url=" + "" + "&hashtags=" + hashtags)
		#If TARGET = "android"
			OpenUrl("http://twitter.com/share?screen_name=" + twitter_name + "&text=" + twitter_text + "&url=" + "" + "&hashtags=" + hashtags)
		#Else
			LaunchBrowser("http://twitter.com/share?screen_name=" + twitter_name + "&text=" + twitter_text + "&url=" + "" + "&hashtags=" + hashtags)
		#EndIf
	End Function
End Class

Function LaunchBrowser:Void(address:String, windowName:String = "_blank")
	#If TARGET = "html5" Or TARGET = "glfw"
		launchBrowser(address, windowName)
	'#Elseif TARGET = "android"
	'	OpenURL("http://twitter.com/share?screen_name=" + twitter_name + "&text=" + twitter_text + "&url=" + "" + "&hashtags=" + hashtags)
	#Endif
End Function


Function CreateCross1:List<Polygon>()

    'Set scale
    Local scale:Float = 64.0

    'Create Polygon List
    Local pList:List<Polygon> = New List<Polygon>

    'Create vertices array
    Local vertices:b2Vec2[]

    'Polygon1
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(115.500/scale, 14.500/scale)
    vertices[1] = New b2Vec2(-14.500/scale, 14.500/scale)
    vertices[2] = New b2Vec2(-115.500/scale, -14.500/scale)
    vertices[3] = New b2Vec2(115.500/scale, -14.500/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon2
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-14.500/scale, 115.500/scale)
    vertices[1] = New b2Vec2(-14.500/scale, 14.500/scale)
    vertices[2] = New b2Vec2(14.500/scale, 14.500/scale)
    vertices[3] = New b2Vec2(14.500/scale, 115.500/scale)
    pList.AddLast(New Polygon(vertices, 4))

    'Polygon3
    vertices = New b2Vec2[3]
    vertices[0] = New b2Vec2(-115.500/scale, -14.500/scale)
    vertices[1] = New b2Vec2(-14.500/scale, 14.500/scale)
    vertices[2] = New b2Vec2(-115.500/scale, 14.500/scale)
    pList.AddLast(New Polygon(vertices, 3))

    'Polygon4
    vertices = New b2Vec2[4]
    vertices[0] = New b2Vec2(-14.500/scale, -14.500/scale)
    vertices[1] = New b2Vec2(-14.500/scale, -115.500/scale)
    vertices[2] = New b2Vec2(14.500/scale, -115.500/scale)
    vertices[3] = New b2Vec2(14.500/scale, -14.500/scale)
    pList.AddLast(New Polygon(vertices, 4))

	Return pList

End Function

	


