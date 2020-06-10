local ESC = core.formspec_escape

local link_color = "<tag name=action color=#ffffcc hovercolor=#ffff00>"
local header_fs = "hypertext[0.2,0.2;8,1;;<big><b>Tutoriel</b></big>]"
local back_fs = "hypertext[12.8,0.2;3,1;back;" .. link_color .. "<action name=>< Retour</big>]"

local chapters = {
	{"Les différents modes de jeu", [[
		<b>Partie solo – Choix de la carte I</b>

		Ce mode de jeu vous permet de jouer, d’explorer, de construire ou réaliser des simulations d’aléas
		climatiques seul(e) sur une carte en 3D.
		Dès lors que vous avez commandé une carte via le GAR ou Eduthèque elle s’affiche dans la liste des
		cartes de l’onglet « Partie solo ». La carte passe par plusieurs états avant de pouvoir être lancée.
		Si la carte est « <style color=orange>en préparation</style> » cela veut dire que les services IGN sont en train de la générer avec
		toutes les données et options que vous avez choisi sur le site Minetest à la Carte. Vous devez donc
		attendre la fin de cette génération qui peut prendre plusieurs minutes. Pour mettre à jour la liste et
		le statut de votre carte, cliquez sur le bouton « Mettre à jour la liste ».
		Si la carte est « <style color=yellow>prête</style> » cela veut dire que vous pouvez la télécharger depuis les serveurs IGN.
		Sélectionnez la carte dans la liste puis cliquez sur le bouton « Télécharger ». La carte est téléchargée
		et installée automatiquement sur votre poste. Le jeu vous demande ensuite de lui donner un nom
		pour finaliser l’installation.
		Si la carte est « <style color=#00ff00>installée</style> » cela veut dire que celle-ci est installée sur votre poste et prête à être
		utilisée. Cliquez sur la carte dans la liste puis sur le bouton « Choisir cette carte » ou double-cliquez
		sur la carte dans la liste pour passer à l’étape suivante.

		<b>Partie solo – Choix de la carte II</b>

		Une fois que vous avez choisi votre carte un deuxième écran apparaît. Plusieurs options s’offrent à vous :
		« Jouer en 3D » : Ce bouton lancera le jeu IGN-kidscode. Vous apparaîtrez au sein de la carte 3D avec
		votre avatar et un tutoriel vous accueillera dans le jeu pour vous expliquer les bases du logiciel.
		« Cartographier en 2D » : Ce bouton lancera l’outil de cartographie IGN-kidscode. Une nouvelle
		fenêtre va s’ouvrir et votre carte 2D va apparaître au bout de quelques secondes.
	]]},

	{"Créer partie multijoueur", [[
		Ce mode de jeu reprend les mêmes principes que le mode solo en ce qui concerne la liste des cartes,
		leur mise à jour, leur téléchargement et leur installation. Toute carte qui se trouve dans la liste du
		mode solo se trouvera également dans la liste des mondes disponibles pour le mode multijoueur.
		En mode multijoueur une personne (la plupart du temps l’enseignant) sélectionne une carte et doit
		ensuite héberger un serveur pour permettre aux élèves de se connecter au sein du même monde.
	]]},

	{"Rejoindre partie multijoueur", [[
		Lorsqu’un élève lance le jeu il doit cliquer sur l’onglet « Rejoindre une partie multijoueur ».
		Si un serveur a été correctement lancé en suivant les étapes du chapitre « Créer partie
		multijoueur » les élèves le verront affiché dans la liste des serveurs disponible.
		L’élève doit d’abord choisir un pseudonyme puis cliquer sur le serveur précédemment
		démarré par l’enseignant(e). Le serveur s’affichera comme ceci :
		Nom de la carte + pseudonyme/nom de l’enseignant.
		L’élève n’a plus qu’à cliquer sur « Rejoindre la partie » pour se connecter au monde.
	]]},
}

return {
	name = "help",
	caption = fgettext("Tutoriel"),
	cbf_formspec = function(tabview, name, tabdata)
		tabdata.help = tabdata.help or 0
		local fs = ""

		if tabdata.help == 0 then
			for i = 1, #chapters do
				fs = fs .. "hypertext[0.2," .. (0.2 + (i * 0.8)) ..
					";10,1;chapter_" .. i .. ";" .. link_color ..
					"<big><action name=>" .. i .. ". " .. ESC(chapters[i][1]) .. "</action></big>]"
			end
		else
			local chapter =  chapters[tabdata.help]
			fs = fs .. back_fs ..
				"hypertext[0.2,1" ..
				";13.5,8.2;;<big>" .. chapter[1] .. "</big>\n\n" .. ESC(chapter[2]) .. "]"
		end

		return header_fs .. fs
	end,

	cbf_button_handler = function(tabview, fields, tabname, tabdata)
		if fields.back then
			tabdata.help = 0
			return true
		elseif next(fields):find"chapter" then
			tabdata.help = tonumber(next(fields):match("%d+"))
			return true
		end

		return true
	end,
}
