import React from 'react';

const SystemDiagram = () => {
  return (
    <div className="w-full min-h-screen bg-white p-8">
      <div className="max-w-[1400px] mx-auto">
        {/* Titre */}
        <div className="text-center mb-8 pb-4 border-b-2 border-gray-800">
          <h1 className="text-2xl font-bold text-gray-900 mb-2">
            DIAGRAMME DE PRINCIPE DU SÉCHOIR SOLAIRE HYBRIDE À HYDROGÈNE
          </h1>
          <p className="text-sm text-gray-700 font-semibold">
            Système de séchage à énergie renouvelable avec production et utilisation d'hydrogène
          </p>
        </div>

        <svg viewBox="0 0 1400 1100" className="w-full border border-gray-300">
          <defs>
            {/* Arrow markers */}
            <marker id="arrowOrange" markerWidth="8" markerHeight="8" refX="7" refY="3" orient="auto">
              <polygon points="0 0, 8 3, 0 6" fill="#ff8c00" />
            </marker>
            <marker id="arrowBlue" markerWidth="8" markerHeight="8" refX="7" refY="3" orient="auto">
              <polygon points="0 0, 8 3, 0 6" fill="#1e90ff" />
            </marker>
            <marker id="arrowGreen" markerWidth="8" markerHeight="8" refX="7" refY="3" orient="auto">
              <polygon points="0 0, 8 3, 0 6" fill="#228b22" />
            </marker>
          </defs>

          {/* Légende */}
          <g transform="translate(50, 30)">
            <text x="0" y="0" fontSize="14" fontWeight="bold" fill="#000">LÉGENDE :</text>
            <line x1="0" y1="20" x2="60" y2="20" stroke="#ff8c00" strokeWidth="2.5" markerEnd="url(#arrowOrange)"/>
            <text x="70" y="25" fontSize="12" fill="#000">Flux énergétique</text>
            
            <line x1="0" y1="45" x2="60" y2="45" stroke="#1e90ff" strokeWidth="2.5" markerEnd="url(#arrowBlue)"/>
            <text x="70" y="50" fontSize="12" fill="#000">Flux d'air</text>
            
            <line x1="0" y1="70" x2="60" y2="70" stroke="#228b22" strokeWidth="2.5" markerEnd="url(#arrowGreen)"/>
            <text x="70" y="75" fontSize="12" fill="#000">Flux de commande</text>
          </g>

          {/* ========== 1. CHAÎNE ÉNERGÉTIQUE (Haut) ========== */}
          <text x="700" y="140" textAnchor="middle" fontSize="15" fontWeight="bold" fill="#000">1. CHAÎNE ÉNERGÉTIQUE</text>

          {/* Panneaux PV */}
          <rect x="100" y="160" width="140" height="70" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="170" y="185" textAnchor="middle" fontSize="12" fontWeight="bold">Panneaux</text>
          <text x="170" y="202" textAnchor="middle" fontSize="12" fontWeight="bold">Photovoltaïques</text>
          <text x="170" y="219" textAnchor="middle" fontSize="10" fill="#666">Énergie solaire PV</text>

          {/* Flèche */}
          <line x1="240" y1="195" x2="290" y2="195" stroke="#ff8c00" strokeWidth="2.5" markerEnd="url(#arrowOrange)"/>
          <text x="265" y="188" textAnchor="middle" fontSize="9" fill="#ff8c00">DC</text>

          {/* MPPT */}
          <rect x="290" y="160" width="120" height="70" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="350" y="185" textAnchor="middle" fontSize="12" fontWeight="bold">Régulateur</text>
          <text x="350" y="202" textAnchor="middle" fontSize="12" fontWeight="bold">MPPT</text>
          <text x="350" y="219" textAnchor="middle" fontSize="10" fill="#666">Optimisation</text>

          {/* Flèche */}
          <line x1="410" y1="195" x2="460" y2="195" stroke="#ff8c00" strokeWidth="2.5" markerEnd="url(#arrowOrange)"/>

          {/* Batterie 12V */}
          <rect x="460" y="160" width="120" height="70" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="520" y="185" textAnchor="middle" fontSize="12" fontWeight="bold">Batterie</text>
          <text x="520" y="202" textAnchor="middle" fontSize="12" fontWeight="bold">12 V</text>
          <text x="520" y="219" textAnchor="middle" fontSize="10" fill="#666">Stockage élec.</text>

          {/* Flèche vers électrolyseur */}
          <line x1="520" y1="230" x2="520" y2="280" stroke="#ff8c00" strokeWidth="2.5" markerEnd="url(#arrowOrange)"/>
          <text x="535" y="260" fontSize="9" fill="#ff8c00">12V</text>

          {/* Électrolyseur */}
          <rect x="450" y="280" width="140" height="70" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="520" y="305" textAnchor="middle" fontSize="12" fontWeight="bold">Électrolyseur</text>
          <text x="520" y="322" textAnchor="middle" fontSize="12" fontWeight="bold">d'hydrogène</text>
          <text x="520" y="339" textAnchor="middle" fontSize="10" fill="#666">H₂O → H₂ + O₂</text>

          {/* Flèche */}
          <line x1="590" y1="315" x2="650" y2="315" stroke="#ff8c00" strokeWidth="2.5" markerEnd="url(#arrowOrange)"/>
          <text x="620" y="308" textAnchor="middle" fontSize="9" fill="#ff8c00">H₂</text>

          {/* Réservoir H2 */}
          <rect x="650" y="280" width="140" height="70" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="720" y="305" textAnchor="middle" fontSize="12" fontWeight="bold">Réservoir de</text>
          <text x="720" y="322" textAnchor="middle" fontSize="12" fontWeight="bold">stockage H₂</text>
          <text x="720" y="339" textAnchor="middle" fontSize="10" fill="#666">Haute pression</text>

          {/* Flèche */}
          <line x1="790" y1="315" x2="850" y2="315" stroke="#ff8c00" strokeWidth="2.5" markerEnd="url(#arrowOrange)"/>
          <text x="820" y="308" textAnchor="middle" fontSize="9" fill="#ff8c00">H₂</text>

          {/* Brûleur hybride */}
          <rect x="850" y="280" width="140" height="70" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="920" y="300" textAnchor="middle" fontSize="12" fontWeight="bold">Brûleur</text>
          <text x="920" y="317" textAnchor="middle" fontSize="12" fontWeight="bold">hybride</text>
          <text x="920" y="334" textAnchor="middle" fontSize="10" fill="#666">H₂ + GPL (secours)</text>
          <text x="920" y="348" textAnchor="middle" fontSize="9" fill="#666">Combustion</text>

          {/* Flèche vers capteur thermique */}
          <line x1="920" y1="350" x2="920" y2="400" stroke="#ff8c00" strokeWidth="2.5" markerEnd="url(#arrowOrange)"/>
          <text x="935" y="380" fontSize="9" fill="#ff8c00">Chaleur</text>

          {/* Capteur solaire thermique */}
          <rect x="850" y="400" width="140" height="80" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="920" y="420" textAnchor="middle" fontSize="12" fontWeight="bold">Capteur solaire</text>
          <text x="920" y="437" textAnchor="middle" fontSize="12" fontWeight="bold">thermique hybride</text>
          <text x="920" y="454" textAnchor="middle" fontSize="10" fill="#666">Combustion +</text>
          <text x="920" y="468" textAnchor="middle" fontSize="10" fill="#666">Rayonnement solaire</text>

          {/* ========== 2. FLUX D'AIR (Milieu) ========== */}
          <text x="700" y="540" textAnchor="middle" fontSize="15" fontWeight="bold" fill="#000">2. FLUX D'AIR</text>

          {/* Entrée air extérieur */}
          <rect x="100" y="560" width="140" height="60" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="170" y="580" textAnchor="middle" fontSize="12" fontWeight="bold">Entrée d'air</text>
          <text x="170" y="597" textAnchor="middle" fontSize="12" fontWeight="bold">extérieur</text>
          <text x="170" y="612" textAnchor="middle" fontSize="10" fill="#666">Air ambiant froid</text>

          {/* Flèche bleue */}
          <line x1="240" y1="590" x2="400" y2="440" stroke="#1e90ff" strokeWidth="2.5" markerEnd="url(#arrowBlue)"/>
          <text x="310" y="510" fontSize="9" fill="#1e90ff">Air froid</text>

          {/* Note sur capteur */}
          <text x="920" y="495" textAnchor="middle" fontSize="9" fill="#1e90ff" fontWeight="bold">4 ventilateurs</text>

          {/* Flèche du capteur vers chambre */}
          <line x1="990" y1="440" x2="1050" y2="590" stroke="#1e90ff" strokeWidth="2.5" markerEnd="url(#arrowBlue)"/>
          <text x="1030" y="510" fontSize="9" fill="#1e90ff">Air chaud</text>

          {/* Chambre de séchage */}
          <rect x="1050" y="560" width="180" height="90" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="1140" y="580" textAnchor="middle" fontSize="12" fontWeight="bold">Chambre de séchage</text>
          <text x="1140" y="597" textAnchor="middle" fontSize="12" fontWeight="bold">vitrée</text>
          <text x="1140" y="614" textAnchor="middle" fontSize="10" fill="#666">5 plateaux métalliques</text>
          <text x="1140" y="628" textAnchor="middle" fontSize="10" fill="#666">noirs (effet de serre)</text>
          <text x="1140" y="642" textAnchor="middle" fontSize="9" fill="#666">Produits agricoles</text>

          {/* Flèche vers cheminée */}
          <line x1="1140" y1="650" x2="1140" y2="710" stroke="#1e90ff" strokeWidth="2.5" markerEnd="url(#arrowBlue)"/>
          <text x="1155" y="685" fontSize="9" fill="#1e90ff">Air humide</text>

          {/* Cheminée extraction */}
          <rect x="1060" y="710" width="160" height="70" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="1140" y="730" textAnchor="middle" fontSize="12" fontWeight="bold">Cheminée</text>
          <text x="1140" y="747" textAnchor="middle" fontSize="12" fontWeight="bold">d'extraction</text>
          <text x="1140" y="764" textAnchor="middle" fontSize="10" fill="#666">1 ventilateur</text>
          <text x="1140" y="778" textAnchor="middle" fontSize="9" fill="#666">d'extraction</text>

          {/* ========== 3. CHAÎNE DE COMMANDE (Bas gauche) ========== */}
          <text x="350" y="540" textAnchor="middle" fontSize="15" fontWeight="bold" fill="#000">3. CHAÎNE DE COMMANDE</text>

          {/* Capteurs */}
          <rect x="100" y="680" width="160" height="100" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="180" y="700" textAnchor="middle" fontSize="12" fontWeight="bold">Capteurs</text>
          <text x="180" y="720" textAnchor="middle" fontSize="10" fill="#666">• Temp. capteur</text>
          <text x="180" y="735" textAnchor="middle" fontSize="10" fill="#666">• Temp. chambre</text>
          <text x="180" y="750" textAnchor="middle" fontSize="10" fill="#666">• Humidité</text>
          <text x="180" y="765" textAnchor="middle" fontSize="10" fill="#666">• Temp. produit</text>

          {/* Flèche verte vers Arduino */}
          <line x1="260" y1="730" x2="330" y2="730" stroke="#228b22" strokeWidth="2.5" markerEnd="url(#arrowGreen)"/>
          <text x="295" y="723" textAnchor="middle" fontSize="9" fill="#228b22">Mesures</text>

          {/* Arduino Mega */}
          <rect x="330" y="680" width="140" height="100" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="400" y="705" textAnchor="middle" fontSize="12" fontWeight="bold">Arduino Mega</text>
          <text x="400" y="722" textAnchor="middle" fontSize="12" fontWeight="bold">Microcontrôleur</text>
          <text x="400" y="742" textAnchor="middle" fontSize="10" fill="#666">Traitement données</text>
          <text x="400" y="757" textAnchor="middle" fontSize="10" fill="#666">Algorithme contrôle</text>
          <text x="400" y="772" textAnchor="middle" fontSize="9" fill="#666">Alimenté par batterie</text>

          {/* Flèche d'alimentation batterie vers Arduino */}
          <line x1="520" y1="230" x2="400" y2="680" stroke="#ff8c00" strokeWidth="2" strokeDasharray="4,4"/>
          <text x="450" y="450" fontSize="9" fill="#ff8c00">12V</text>

          {/* Flèche verte vers Relais */}
          <line x1="470" y1="730" x2="540" y2="730" stroke="#228b22" strokeWidth="2.5" markerEnd="url(#arrowGreen)"/>
          <text x="505" y="723" textAnchor="middle" fontSize="9" fill="#228b22">Signaux</text>

          {/* Relais de commande */}
          <rect x="540" y="680" width="140" height="100" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="610" y="705" textAnchor="middle" fontSize="12" fontWeight="bold">Relais de</text>
          <text x="610" y="722" textAnchor="middle" fontSize="12" fontWeight="bold">commande</text>
          <text x="610" y="742" textAnchor="middle" fontSize="10" fill="#666">Module de</text>
          <text x="610" y="757" textAnchor="middle" fontSize="10" fill="#666">puissance</text>
          <text x="610" y="772" textAnchor="middle" fontSize="9" fill="#666">Interface actionneurs</text>

          {/* Actionneurs - Bloc ventilateurs */}
          <rect x="750" y="650" width="160" height="60" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="830" y="670" textAnchor="middle" fontSize="11" fontWeight="bold">Ventilateurs</text>
          <text x="830" y="687" textAnchor="middle" fontSize="10" fill="#666">5 internes + 1 extraction</text>
          <text x="830" y="701" textAnchor="middle" fontSize="9" fill="#666">Contrôle vitesse PWM</text>

          {/* Flèche vers ventilateurs */}
          <line x1="680" y1="690" x2="750" y2="680" stroke="#228b22" strokeWidth="2.5" markerEnd="url(#arrowGreen)"/>

          {/* Actionneurs - Électrovanne */}
          <rect x="750" y="740" width="160" height="60" fill="none" stroke="#000" strokeWidth="2"/>
          <text x="830" y="760" textAnchor="middle" fontSize="11" fontWeight="bold">Électrovanne</text>
          <text x="830" y="777" textAnchor="middle" fontSize="10" fill="#666">H₂ / GPL</text>
          <text x="830" y="791" textAnchor="middle" fontSize="9" fill="#666">Contrôle débit gaz</text>

          {/* Flèche vers électrovanne */}
          <line x1="680" y1="750" x2="750" y2="770" stroke="#228b22" strokeWidth="2.5" markerEnd="url(#arrowGreen)"/>

          {/* Connexion électrovanne vers brûleur */}
          <line x1="830" y1="740" x2="920" y2="350" stroke="#228b22" strokeWidth="2" strokeDasharray="4,4"/>
          <text x="880" y="540" fontSize="9" fill="#228b22">Commande</text>

          {/* Connexion ventilateurs vers capteur et chambre */}
          <line x1="830" y1="650" x2="920" y2="480" stroke="#228b22" strokeWidth="2" strokeDasharray="4,4"/>
          <line x1="890" y1="670" x2="1140" y2="650" stroke="#228b22" strokeWidth="2" strokeDasharray="4,4"/>

          {/* Titre système global */}
          <text x="700" y="1050" textAnchor="middle" fontSize="16" fontWeight="bold" fill="#000">SYSTÈME INTÉGRÉ DE SÉCHAGE SOLAIRE HYBRIDE À HYDROGÈNE</text>
          <text x="700" y="1075" textAnchor="middle" fontSize="11" fill="#666">Production autonome d'énergie · Stockage H₂ · Contrôle automatisé · Séchage haute performance</text>

        </svg>

        {/* Notes techniques */}
        <div className="mt-8 border-t-2 border-gray-800 pt-6">
          <h2 className="text-lg font-bold mb-4">NOTES TECHNIQUES</h2>
          <div className="grid grid-cols-3 gap-6 text-xs">
            <div>
              <h3 className="font-bold mb-2">Chaîne énergétique</h3>
              <p className="mb-1">• Conversion photovoltaïque : DC 12V</p>
              <p className="mb-1">• Production H₂ : électrolyse de l'eau</p>
              <p className="mb-1">• Combustion hybride : H₂ + GPL secours</p>
              <p>• Rendement thermique : capteur solaire + brûleur</p>
            </div>
            <div>
              <h3 className="font-bold mb-2">Flux d'air</h3>
              <p className="mb-1">• Préchauffage : capteur solaire thermique</p>
              <p className="mb-1">• Distribution : 4 ventilateurs 12V</p>
              <p className="mb-1">• Séchage : 5 plateaux, effet de serre</p>
              <p>• Extraction : 1 ventilateur + cheminée naturelle</p>
            </div>
            <div>
              <h3 className="font-bold mb-2">Système de contrôle</h3>
              <p className="mb-1">• Microcontrôleur : Arduino Mega</p>
              <p className="mb-1">• 4 capteurs de température/humidité</p>
              <p className="mb-1">• Contrôle PWM des ventilateurs</p>
              <p>• Gestion automatique H₂/GPL selon besoin thermique</p>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default SystemDiagram;