
#include <chrono>
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

#include "audioplus/wav.h"
#include "audioplus/audio.h"
#include "audioplus/midi.h"

using namespace audioplus;

std::string make_fname(std::string const& dir)
{
    std::ostringstream out;
    out << dir << "/" << time(0) << ".wav";
    return out.str();
}

bool matches_patterns(std::string const& s, 
    std::vector<std::string> const& patterns)
{
    bool match = false;
    for(auto & p : patterns)
    {
        match |= (s.find(p) < s.size());
    }
    return match;
}

struct AutoRecorder
{
    std::string out_dir;
    std::vector<std::string> patterns;

    AudioSession audio_session;
    AudioStream audio_stream;

    MidiSession midi_session;
    MidiInputStream midi_stream;

    float sample_rate = 0;

    WavStream<std::ofstream> wav;
    bool recording = false;
    int16_t min_amp = 10;
    float idle = 999;
    float max_idle = 2.0;
    double t0 = 0;

    bool dead = false;

    bool running()
    { 
        if(!audio_stream.running()) { return false; }
        bool was_running = !dead;
        dead = true;
        return was_running;
    }

    void restart()
    {
        wav.finish();
        recording = false;

        // close before restarting session
        audio_stream.close();
        midi_stream.close();
        // session restart refreshes devices
        audio_session.restart();
        midi_session.restart();

        std::cout << "audio devices: " << (audio_session.end() - audio_session.begin()) << std::endl;
        std::cout << "---" << std::endl;
        for(AudioDevice device : audio_session)
        {
            std::cout << "audio device: " << device.name() << std::endl;
            std::cout << "in channels: " << device.max_input_channels() << std::endl;
            std::cout << "out channels: " << device.max_output_channels() << std::endl;
            std::cout << "sample rate: " << device.default_sample_rate() << std::endl;
            std::cout << "---" << std::endl;
        }
        for(AudioDevice device : audio_session)
        {
            if(device.max_input_channels() < 1) { continue; }
            if(!matches_patterns(device.name(), patterns)) { continue; }

            AudioStream::Config config;
            config.on_audio(this);
            config.chunk_frames = 4096;

            config.output_channels = 0;
            config.input_channels = 1;
            
            config.input_device = device;
            config.output_device = device;

            audio_stream = audio_session.open(config);
            
            sample_rate = config.sample_rate; // updated in open()

            audio_stream.start();
            dead = false;

            break;
        }

        std::cout << "midi devices: " << (midi_session.end() - midi_session.begin()) << std::endl;
        std::cout << "---" << std::endl;
        for(MidiDevice device : midi_session)
        {
            std::cout << "midi device: " << device.name() << std::endl;
            std::cout << "---" << std::endl;
        }
        for(MidiDevice device : midi_session)
        {
            if(!device.has_input()) { continue; }
            if(!matches_patterns(device.name(), patterns)) { continue; }

            MidiInputStream::Config config;
            config.device = device;
            config.clock_time(this);
            midi_stream = midi_session.open(config);

            break;
        }
    }

    int32_t clock_time()
    {
        double t = audio_stream.clock_time();
        return int32_t((t - t0) * 1000);
    }

    int on_audio(int16_t const* in, int16_t * out, int frames, AudioStream::Status const& status)
    {
        if(frames > 0) { dead = false; }

        double t = status.input_hw_time;

        int16_t amp = 0;
        for(int i=0 ; i<frames ; i++)
            amp = std::max<int16_t>(amp, std::abs(in[i]));

        constexpr int max_midi = 128;
        MidiMsg msg[max_midi];
        int msg_count = midi_stream.read(msg, max_midi);

        if(amp > min_amp) { idle = 0; }
        else { idle += frames / sample_rate; }

        if(idle <= max_idle && !recording)
        {
            std::string fname = make_fname(out_dir);
            wav = make_wav_stream(std::ofstream(
                fname, std::ios::binary ));

            WavHeader header;
            header.sample_rate = sample_rate;
            header.channels = 2;
            header.dtype = WavHeader::Int16;
            wav << header;

            recording = true;
            std::cout << "start recording: " << fname << std::endl;

            t0 = t;
        }
        if((idle > max_idle || (t-t0 > 10)) && recording)
        {
            wav.finish();
            recording = false;
            std::cout << "stop recording" << std::endl;
        }

        if(recording)
        {
            std::vector<int16_t> buffer(frames * 2);
            for(int i=0 ; i<frames ; i++)
            {
                buffer[i * 2] = in[i];
                buffer[i * 2 + 1] = -1;
            }
            for(int i=0 ; i<msg_count ; i++)
            {
                double mt = msg[i].timestamp / 1000.0;
                int slot = (mt + t0 - t) * sample_rate / 4;
                slot = std::min(slot, frames/4-msg_count+i);
                slot = std::max(slot, i);
                for(int j=0 ; j<4 ; j++)
                    buffer[((slot * 4) + j) * 2 + 1] = msg[i].data[j];
            }
            wav << buffer;
        }

        return 0;
    }
};


int main(int argc, char ** argv)
{
    if(argc < 3)
    {
        std::cerr << "usage: pianobox out_dir patterns..." << std::endl;
        return -1;
    }

    AutoRecorder recorder;

    recorder.out_dir = argv[1];
    for(int i=2 ; i<argc ; i++)
        recorder.patterns.emplace_back(argv[i]);

    while(true)
    {
        if(!recorder.running()) { recorder.restart(); }
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(2s);
    }

    return 0;
}
