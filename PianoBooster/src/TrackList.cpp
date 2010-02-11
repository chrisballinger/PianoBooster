/*********************************************************************************/
/*!
@file           TrackList.cpp

@brief          The Design.

@author         L. J. Barman

    Copyright (c)   2008-2009, L. J. Barman, all rights reserved

    This file is part of the PianoBooster application

    PianoBooster is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PianoBooster is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PianoBooster.  If not, see <http://www.gnu.org/licenses/>.

*/
/*********************************************************************************/

#include <QtGui>

#include "TrackList.h"
#include "Song.h"
#include "Settings.h"

void CTrackList::init(CSong* songObj, CSettings* settings)
{
    m_song = songObj;
    m_settings = settings;
}

void CTrackList::clear()
{
    int chan;

    for (chan = 0; chan < MAX_MIDI_CHANNELS; chan++)
    {
        m_midiActiveChannels[chan] = false;
        m_midiFirstPatchChannels[chan] = -1;
        for (int i = 0; i < MAX_MIDI_NOTES; i++)
            m_noteFrequency[chan][i]=0;
    }
    m_trackQtList.clear();
}

void CTrackList::currentRowChanged(int currentRow)
{
    if (!m_song) return;
    if (currentRow >= m_trackQtList.size()|| currentRow < 0)
        return;

    m_song->setActiveChannel(m_trackQtList[currentRow].midiChannel);
}

void CTrackList::examineMidiEvent(CMidiEvent event)
{
    int chan;
    chan = event.channel();
    assert (chan < MAX_MIDI_CHANNELS && chan >= 0);
    if (chan < MAX_MIDI_CHANNELS && chan >= 0)
    {
        if (event.type() == MIDI_NOTE_ON)
        {
            m_midiActiveChannels[chan] = true;
            // count each note so we can guess the key signature
            if (event.note() >= 0 && event.note() < MAX_MIDI_NOTES)
                m_noteFrequency[chan][event.note()]++;

            // If we have a note and no patch then default to grand piano patch
            if (m_midiFirstPatchChannels[chan] == -1)
                m_midiFirstPatchChannels[chan] = GM_PIANO_PATCH;

        }

        if (event.type() == MIDI_PROGRAM_CHANGE && m_midiActiveChannels[chan] == false)
            m_midiFirstPatchChannels[chan] = event.programme();
    }
}

// Returns true if there is a piano part on channels 3 & 4
bool CTrackList::pianoPartConvetionTest()
{
    if ((m_midiFirstPatchChannels[CONVENTION_LEFT_HAND_CHANNEL] == GM_PIANO_PATCH &&
         m_midiActiveChannels[CONVENTION_LEFT_HAND_CHANNEL] == true &&
         m_midiFirstPatchChannels[CONVENTION_RIGHT_HAND_CHANNEL]  <= GM_PIANO_PATCH))
            return true;

    if (m_midiFirstPatchChannels[CONVENTION_RIGHT_HAND_CHANNEL] == GM_PIANO_PATCH &&
         m_midiActiveChannels[CONVENTION_RIGHT_HAND_CHANNEL] == true &&
         m_midiFirstPatchChannels[CONVENTION_LEFT_HAND_CHANNEL]  <= GM_PIANO_PATCH)
            return true;

    if (m_midiFirstPatchChannels[CONVENTION_LEFT_HAND_CHANNEL] == GM_PIANO_PATCH &&
        m_midiActiveChannels[CONVENTION_LEFT_HAND_CHANNEL] == true &&
        m_midiFirstPatchChannels[CONVENTION_RIGHT_HAND_CHANNEL] <= GM_PIANO_PATCH)
            return true;
    return false;
}

int CTrackList::guessKeySignature(int chanA, int chanB)
{
    int chan;
    int i;
    int keySignature = 0;
    int highScore = 0;
    int scale[MIDI_OCTAVE];
    for (i=0; i < MIDI_OCTAVE; i++)
        scale[i] = 0;
    for (chan = 0 ; chan < MAX_MIDI_CHANNELS; chan++)
    {
        if (chanA == -1 || chan == chanA || chan == chanB)
        {
            for (int note = 0; note < MAX_MIDI_NOTES; note++)
                scale[note % MIDI_OCTAVE] += m_noteFrequency[chan][note];
        }
    }

    for (i = 0; i < MIDI_OCTAVE; i++)
    {
        int score = 0;
        struct {
            int offset;
            int key;
        } keyLookUp[MIDI_OCTAVE] =
            {
                {0,  0}, // 0  C
                {7,  1}, // 1  G  1#
                {5, -1}, // 2  F  1b
                {2,  2}, // 3  D  2#
                {10,-2}, // 4  Bb 2b
                {9,  3}, // 5  A  3#
                {3, -3}, // 6  Eb 3b
                {4,  4}, // 7  E  4#
                {8, -4}, // 8  Ab 4b
                {11, 5}, // 9  B  5#
                {1, -5}, // 10 Db 5b
                {6,  6}, // 11 F# 6#
            };

        int idx = keyLookUp[i].offset;
        score += scale[(idx + 0 )%MIDI_OCTAVE]; // First note in the scale
        score += scale[(idx + 2 )%MIDI_OCTAVE]; // Tone
        score += scale[(idx + 4 )%MIDI_OCTAVE]; // Tone
        score += scale[(idx + 5 )%MIDI_OCTAVE]; // Semi tone
        score += scale[(idx + 7 )%MIDI_OCTAVE]; // Tone
        score += scale[(idx + 9 )%MIDI_OCTAVE]; // Tone
        score += scale[(idx + 11)%MIDI_OCTAVE]; // Tone
                                                // the Last note don't count it

        if (score > highScore)
        {
            highScore = score;
            keySignature = keyLookUp[i].key;
        }
        /*
        printf("key %2d score %3d :: ", keyLookUp[i].key, score);
        for (int j=0; j < MIDI_OCTAVE; j++)
            printf(" %d", scale[(keyLookUp[i].offset + j)%MIDI_OCTAVE]);
        printf("\n");
        */
    }
    return keySignature;
}

void CTrackList::refresh()
{
    int chan;
    int rowCount = 0;
    m_trackQtList.clear();

    for (chan = 0; chan < MAX_MIDI_CHANNELS; chan++)
    {
        if (m_midiActiveChannels[chan] == true)
        {
           CTrackListItem trackItem;
            trackItem.midiChannel =  chan;
            m_trackQtList.append(trackItem);
            rowCount++;
        }
    }
    if (pianoPartConvetionTest())
    {
        if (CNote::bothHandsChan() == -2 ) // -2 for not set -1 for not used
            CNote::setChannelHands(CONVENTION_LEFT_HAND_CHANNEL, CONVENTION_RIGHT_HAND_CHANNEL);
        m_song->setActiveChannel(CNote::bothHandsChan());
        ppLogInfo("Active both");
    }
    else
    {
        if (m_trackQtList.count() > 0)
        {
            m_song->setActiveChannel(m_trackQtList[0].midiChannel);
        }
    }

    if (CStavePos::getKeySignature() == NOT_USED)
        CStavePos::setKeySignature(guessKeySignature(CNote::rightHandChan(),CNote::leftHandChan()), 0);

    int goodChan = -1;
    // Find an unused channel that we can use for the keyboard
    for (chan = 0; chan < MAX_MIDI_CHANNELS; chan++)
    {
        if (m_midiActiveChannels[chan] == false)
        {
            if (goodChan != -1)
            {
                m_song->setPianistChannels(goodChan,chan);
                ppLogInfo("Using Pianist Channels %d + %d", goodChan +1, chan +1);
                return;
            }
            goodChan = chan;
        }
    }
    // As we have not returned we have not found to empty channels to use
    if (goodChan == -1)
        goodChan = 15 -1;
    m_song->setPianistChannels(goodChan,16-1);
}

int CTrackList::getActiveItemIndex()
{
    int chan;
    for (int i = 0; i < m_trackQtList.size(); ++i)
    {
        chan = m_trackQtList.at(i).midiChannel;
        if (chan == CNote::rightHandChan() )
            return i;
    }
    return 0; // Not found so return first item on the list
}

QStringList CTrackList::getAllChannelProgramNames(bool raw)
{
    QStringList items;
    int chan;
    QString text;
    QString hand;

    for (int i = 0; i < m_trackQtList.size(); ++i)
    {
        hand.clear();
        chan = m_trackQtList.at(i).midiChannel;
        if (raw == false)
        {
            if (CNote::leftHandChan() == chan)
                hand += tr("L");
            if (CNote::rightHandChan() == chan)
                hand += tr("R");
        }
        text = QString::number(chan+1) + hand + " " + getChannelProgramName(chan);
        items += text;
    }
    return items;
}

int CTrackList::getActiveHandIndex(whichPart_t whichPart)
{
    int index = 0;
     for (int i = 0; i < m_trackQtList.size(); ++i)
        if (m_trackQtList.at(i).midiChannel == CNote::getHandChannel( whichPart))
            return index;

    return index;
}

void CTrackList::setActiveHandsIndex(int leftIndex, int rightIndex)
{
    int leftChannel = -1;
    int rightChannel = -1;

    if (leftIndex>=0)
        leftChannel = m_trackQtList.at(leftIndex).midiChannel;
    if (rightIndex>=0)
        rightChannel = m_trackQtList.at(rightIndex).midiChannel;
    m_settings->setChannelHands(leftChannel, rightChannel);
    refresh();
    m_song->rewind();
}

// get the track index number of the selected hand
int CTrackList::getHandTrackIndex(whichPart_t whichPart)
{
    int index = 0;
    int midiHand = CNote::getHandChannel(whichPart);
    for (int i = 0; i < m_trackQtList.size(); ++i)
    {

        if (m_trackQtList.at(i).midiChannel == midiHand)
            return index;
        index++;
    }

    return -1;
}

void CTrackList::changeListWidgetItemView( unsigned int index, QListWidgetItem* listWidgetItem )
{
    int chan = m_trackQtList[index].midiChannel;
    if ( CNote::hasPianoPart( chan ))
    {
        QFont font = listWidgetItem->font();
        if (CNote::rightHandChan() >= 0 && CNote::leftHandChan() >= 0 )
            font.setBold(true);
        listWidgetItem->setFont(font);
        listWidgetItem->setForeground(Qt::darkBlue);
    }
    else if ( m_song->hasPianistKeyboardChannel( chan ) )
        listWidgetItem->setForeground(Qt::lightGray);
}

QString CTrackList::getChannelProgramName(int chan)
{
    if(chan<0 || chan>= static_cast<int>(arraySize(m_midiFirstPatchChannels)))
    {
        assert(true);
        return QString();
    }
    int program = m_midiFirstPatchChannels[chan];

    if (chan==10-1)
        return tr("Drums");
    QString name = getProgramName(program +1); // Skip
    if (name.isEmpty())
        name = tr("Unknown");

    return name;
}

QString CTrackList::getProgramName(int program)
{
    const char * const gmInstrumentNames[] =
    {
                   tr("(None)"),  // Don't use
        /* 1.   */ tr("Grand Piano"),
        /* 2.   */ tr("Bright Piano"),
        /* 3.   */ tr("Electric Grand"),
        /* 4.   */ tr("Honky-tonk Piano"),
        /* 5.   */ tr("Electric Piano 1"),
        /* 6.   */ tr("Electric Piano 2"),
        /* 7.   */ tr("Harpsichord"),
        /* 8.   */ tr("Clavi"),
        /* 9.   */ tr("Celesta"),
        /* 10.  */ tr("Glockenspiel"),
        /* 11.  */ tr("Music Box"),
        /* 12.  */ tr("Vibraphone"),
        /* 13.  */ tr("Marimba"),
        /* 14.  */ tr("Xylophone"),
        /* 15.  */ tr("Tubular Bells"),
        /* 16.  */ tr("Dulcimer"),
        /* 17.  */ tr("Drawbar Organ"),
        /* 18.  */ tr("Percussive Organ"),
        /* 19.  */ tr("Rock Organ"),
        /* 20.  */ tr("Church Organ"),
        /* 21.  */ tr("Reed Organ"),
        /* 22.  */ tr("Accordion"),
        /* 23.  */ tr("Harmonica"),
        /* 24.  */ tr("Tango Accordion"),
        /* 25.  */ tr("Acoustic Guitar (nylon)"),
        /* 26.  */ tr("Acoustic Guitar (steel)"),
        /* 27.  */ tr("Electric Guitar (jazz)"),
        /* 28.  */ tr("Electric Guitar (clean)"),
        /* 29.  */ tr("Electric Guitar (muted)"),
        /* 30.  */ tr("Overdriven Guitar"),
        /* 31.  */ tr("Distortion Guitar"),
        /* 32.  */ tr("Guitar harmonics"),
        /* 33.  */ tr("Acoustic Bass"),
        /* 34.  */ tr("Electric Bass (finger)"),
        /* 35.  */ tr("Electric Bass (pick)"),
        /* 36.  */ tr("Fretless Bass"),
        /* 37.  */ tr("Slap Bass 1"),
        /* 38.  */ tr("Slap Bass 2"),
        /* 39.  */ tr("Synth Bass 1"),
        /* 40.  */ tr("Synth Bass 2"),
        /* 41.  */ tr("Violin"),
        /* 42.  */ tr("Viola"),
        /* 43.  */ tr("Cello"),
        /* 44.  */ tr("Contrabass"),
        /* 45.  */ tr("Tremolo Strings"),
        /* 46.  */ tr("Pizzicato Strings"),
        /* 47.  */ tr("Orchestral Harp"),
        /* 48.  */ tr("Timpani"),
        /* 49.  */ tr("String Ensemble 1"),
        /* 50.  */ tr("String Ensemble 2"),
        /* 51.  */ tr("SynthStrings 1"),
        /* 52.  */ tr("SynthStrings 2"),
        /* 53.  */ tr("Choir Aahs"),
        /* 54.  */ tr("Voice Oohs"),
        /* 55.  */ tr("Synth Voice"),
        /* 56.  */ tr("Orchestra Hit"),
        /* 57.  */ tr("Trumpet"),
        /* 58.  */ tr("Trombone"),
        /* 59.  */ tr("Tuba"),
        /* 60.  */ tr("Muted Trumpet"),
        /* 61.  */ tr("French Horn"),
        /* 62.  */ tr("Brass Section"),
        /* 63.  */ tr("SynthBrass 1"),
        /* 64.  */ tr("SynthBrass 2"),
        /* 65.  */ tr("Soprano Sax"),
        /* 66.  */ tr("Alto Sax"),
        /* 67.  */ tr("Tenor Sax"),
        /* 68.  */ tr("Baritone Sax"),
        /* 69.  */ tr("Oboe"),
        /* 70.  */ tr("English Horn"),
        /* 71.  */ tr("Bassoon"),
        /* 72.  */ tr("Clarinet"),
        /* 73.  */ tr("Piccolo"),
        /* 74.  */ tr("Flute"),
        /* 75.  */ tr("Recorder"),
        /* 76.  */ tr("Pan Flute"),
        /* 77.  */ tr("Blown Bottle"),
        /* 78.  */ tr("Shakuhachi"),
        /* 79.  */ tr("Whistle"),
        /* 80.  */ tr("Ocarina"),
        /* 81.  */ tr("Lead 1 (square)"),
        /* 82.  */ tr("Lead 2 (sawtooth)"),
        /* 83.  */ tr("Lead 3 (calliope)"),
        /* 84.  */ tr("Lead 4 (chiff)"),
        /* 85.  */ tr("Lead 5 (charang)"),
        /* 86.  */ tr("Lead 6 (voice)"),
        /* 87.  */ tr("Lead 7 (fifths)"),
        /* 88.  */ tr("Lead 8 (bass + lead)"),
        /* 89.  */ tr("Pad 1 (new age)"),
        /* 90.  */ tr("Pad 2 (warm)"),
        /* 91.  */ tr("Pad 3 (polysynth)"),
        /* 92.  */ tr("Pad 4 (choir)"),
        /* 93.  */ tr("Pad 5 (bowed)"),
        /* 94.  */ tr("Pad 6 (metallic)"),
        /* 95.  */ tr("Pad 7 (halo)"),
        /* 96.  */ tr("Pad 8 (sweep)"),
        /* 97.  */ tr("FX 1 (rain)"),
        /* 98.  */ tr("FX 2 (soundtrack)"),
        /* 99.  */ tr("FX 3 (crystal)"),
        /* 100. */ tr("FX 4 (atmosphere)"),
        /* 101. */ tr("FX 5 (brightness)"),
        /* 102. */ tr("FX 6 (goblins)"),
        /* 103. */ tr("FX 7 (echoes)"),
        /* 104. */ tr("FX 8 (sci-fi)"),
        /* 105. */ tr("Sitar"),
        /* 106. */ tr("Banjo"),
        /* 107. */ tr("Shamisen"),
        /* 108. */ tr("Koto"),
        /* 109. */ tr("Kalimba"),
        /* 110. */ tr("Bag pipe"),
        /* 111. */ tr("Fiddle"),
        /* 112. */ tr("Shanai"),
        /* 113. */ tr("Tinkle Bell"),
        /* 114. */ tr("Agogo"),
        /* 115. */ tr("Steel Drums"),
        /* 116. */ tr("Woodblock"),
        /* 117. */ tr("Taiko Drum"),
        /* 118. */ tr("Melodic Tom"),
        /* 119. */ tr("Synth Drum"),
        /* 120. */ tr("Reverse Cymbal"),
        /* 121. */ tr("Guitar Fret Noise"),
        /* 122. */ tr("Breath Noise"),
        /* 123. */ tr("Seashore"),
        /* 124. */ tr("Bird Tweet"),
        /* 125. */ tr("Telephone Ring"),
        /* 126. */ tr("Helicopter"),
        /* 127. */ tr("Applause"),
        /* 128. */ tr("Gunshot"),
    };

    if (program >= 0 && program < static_cast<int>(arraySize(gmInstrumentNames)))
        return gmInstrumentNames[program];
    else
        return QString();
}
